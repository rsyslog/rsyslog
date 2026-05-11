#!/bin/bash
. ${srcdir:=.}/diag.sh init
require_plugin imbeats
require_plugin omelasticsearch
check_command_available docker

if ! docker info >/dev/null 2>&1 ; then
	echo "docker daemon not available, skipping test"
	exit 77
fi

export ES_PORT="${ES_PORT:-19200}"
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=imbeats_es_shutdown_empty_check
export FB_IMAGE="${FB_IMAGE:-docker.elastic.co/beats/filebeat:9.2.0}"
export FB_CONTAINER="fb-imbeats-es-${RSYSLOG_DYNNAME}"
export FB_WORKDIR="${TESTTOOL_DIR}/${RSYSLOG_DYNNAME}.filebeat"
if [ -z "$FB_DOCKER_MOUNT_SOURCE" ] && [ -n "$GITHUB_WORKSPACE" ] ; then
	case "$FB_WORKDIR" in
		/rsyslog/*) FB_DOCKER_MOUNT_SOURCE="${GITHUB_WORKSPACE}${FB_WORKDIR#/rsyslog}" ;;
	esac
fi
export FB_DOCKER_MOUNT_SOURCE="${FB_DOCKER_MOUNT_SOURCE:-$FB_WORKDIR}"
export FB_INPUT="${FB_WORKDIR}/input.log"
export FB_CONFIG="${FB_WORKDIR}/filebeat.yml"
export FB_LOG="${FB_WORKDIR}/docker.log"
export RAW_INDEX="rsyslog_testbench_beats_raw"
export AMENDED_INDEX="rsyslog_testbench_beats_amended"
export MARKER="imbeats-elasticsearch"

cleanup_filebeat() {
	docker rm -f "$FB_CONTAINER" >/dev/null 2>&1 || true
	rm -rf "$FB_WORKDIR"
}
trap cleanup_filebeat EXIT

es_count_index() {
	local base="$1"
	local index="$2"
	local response
	local http_status
	local body

	response=$(curl --silent --show-error --write-out '\n%{http_code}' \
		"${base}/${index}/_count")
	http_status=$(printf '%s' "$response" | tail -n 1)
	body=$(printf '%s' "$response" | sed '$d')
	if [ "$http_status" = "404" ]; then
		printf '0\n'
	elif [ "$http_status" = "200" ]; then
		printf '%s' "$body" | "$PYTHON" -c '
import json
import sys

print(json.load(sys.stdin).get("count", 0))
'
	else
		printf '%s es_count_index: unexpected Elasticsearch status %s for %s\n' \
			"$(tb_timestamp)" "$http_status" "$index" >&2
		return 1
	fi
}

imbeats_es_shutdown_empty_check() {
	local base
	local raw_lines
	local amended_lines

	base=$(es_base_url)
	raw_lines=$(es_count_index "$base" "$RAW_INDEX") || return 1
	amended_lines=$(es_count_index "$base" "$AMENDED_INDEX") || return 1
	if [ "$raw_lines" -eq "$NUMMESSAGES" ] && [ "$amended_lines" -eq "$NUMMESSAGES" ]; then
		printf '%s imbeats_es_shutdown_empty_check: success, have %d raw and %d amended documents\n' \
			"$(tb_timestamp)" "$raw_lines" "$amended_lines"
		return 0
	fi
	printf '%s imbeats_es_shutdown_empty_check: have %d raw and %d amended documents, expecting %d each\n' \
		"$(tb_timestamp)" "$raw_lines" "$amended_lines" "$NUMMESSAGES"
	return 1
}

fetch_index() {
	local index="$1"
	local output="$2"
	local base

	base=$(es_base_url)
	curl --silent --show-error "${base}/${index}/_refresh" >/dev/null
	curl --silent --show-error "${base}/${index}/_search?size=${NUMMESSAGES}" > "$output"
}

ensure_elasticsearch_ready
init_elasticsearch
base=$(es_base_url)
curl --silent --show-error -XDELETE "${base}/${RAW_INDEX}" >/dev/null || true
curl --silent --show-error -XDELETE "${base}/${AMENDED_INDEX}" >/dev/null || true

mkdir -p "$FB_WORKDIR/data"
for i in $(seq 0 $((NUMMESSAGES - 1))) ; do
	printf 'msgnum:%08d:\n' "$i"
done > "$FB_INPUT"

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="rawBeatEvent" type="string" string="%msg%")
template(name="amendedBeatEvent" type="subtree" subtree="$!")

ruleset(name="beats_to_es") {
  action(type="omelasticsearch"
         server="127.0.0.1"
         serverport="'$ES_PORT'"
         template="rawBeatEvent"
         searchIndex="'$RAW_INDEX'")

  set $!rsyslog!test!pipeline = "amended";
  set $!rsyslog!test!marker = "'$MARKER'";

  action(type="omelasticsearch"
         server="127.0.0.1"
         serverport="'$ES_PORT'"
         template="amendedBeatEvent"
         searchIndex="'$AMENDED_INDEX'")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="beats_to_es")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

cat > "$FB_CONFIG" <<EOF
filebeat.inputs:
  - type: filestream
    id: imbeats-es-test
    enabled: true
    paths:
      - /work/input.log
    fields_under_root: true
    fields:
      rsyslog_test_case: ${MARKER}

output.logstash:
  hosts: ["127.0.0.1:${RS_PORT}"]

logging.level: info
logging.to_files: false
EOF

if ! docker image inspect "$FB_IMAGE" >/dev/null 2>&1 ; then
	if ! docker pull "$FB_IMAGE" ; then
		echo "docker pull for $FB_IMAGE failed, skipping test"
		exit 77
	fi
fi

docker run -d --rm \
	--name "$FB_CONTAINER" \
	--network host \
	--user root \
	-v "$FB_DOCKER_MOUNT_SOURCE:/work" \
	"$FB_IMAGE" \
	filebeat -e --strict.perms=false -c /work/filebeat.yml > "$FB_LOG"

shutdown_when_empty
wait_shutdown

docker rm -f "$FB_CONTAINER" >/dev/null 2>&1 || true

fetch_index "$RAW_INDEX" "$RSYSLOG_DYNNAME.raw.work"
fetch_index "$AMENDED_INDEX" "$RSYSLOG_DYNNAME.amended.work"

"$PYTHON" - "$RSYSLOG_DYNNAME.raw.work" "$RSYSLOG_DYNNAME.amended.work" "$NUMMESSAGES" "$MARKER" <<'PY' || \
	error_exit 1
import json
import sys

raw_path, amended_path, expected_count, marker = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]


def load_sources(path):
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    hits = data["hits"]["hits"]
    return sorted((hit["_source"] for hit in hits), key=lambda item: item.get("message", ""))


raw = load_sources(raw_path)
amended = load_sources(amended_path)

if len(raw) != expected_count:
    raise SystemExit(f"raw index has {len(raw)} documents, expected {expected_count}")
if len(amended) != expected_count:
    raise SystemExit(f"amended index has {len(amended)} documents, expected {expected_count}")

for idx, (raw_doc, amended_doc) in enumerate(zip(raw, amended)):
    expected_message = f"msgnum:{idx:08d}:"
    if raw_doc.get("message") != expected_message:
        raise SystemExit(f"raw message mismatch at {idx}: {raw_doc.get('message')!r}")
    if amended_doc.get("message") != expected_message:
        raise SystemExit(f"amended message mismatch at {idx}: {amended_doc.get('message')!r}")
    if raw_doc.get("rsyslog_test_case") != marker:
        raise SystemExit(f"raw Filebeat marker missing at {idx}")
    if amended_doc.get("rsyslog_test_case") != marker:
        raise SystemExit(f"amended Filebeat marker changed at {idx}")
    if "rsyslog" in raw_doc:
        raise SystemExit(f"raw document unexpectedly contains rsyslog amendments at {idx}")

    rsyslog_meta = amended_doc.get("rsyslog", {}).get("test", {})
    if rsyslog_meta.get("pipeline") != "amended" or rsyslog_meta.get("marker") != marker:
        raise SystemExit(f"amended rsyslog metadata missing at {idx}: {rsyslog_meta!r}")

    imbeats_meta = amended_doc.get("metadata", {}).get("imbeats", {})
    if imbeats_meta.get("protocol") != "lumberjack-v2":
        raise SystemExit(f"imbeats protocol metadata missing at {idx}: {imbeats_meta!r}")
    if int(imbeats_meta.get("sequence", -1)) < 1:
        raise SystemExit(f"imbeats sequence metadata missing at {idx}: {imbeats_meta!r}")

print("validated raw and amended Beats documents")
PY

exit_test
