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

cleanup_filebeat() {
	docker rm -f "$FB_CONTAINER" >/dev/null 2>&1 || true
	rm -rf "$FB_WORKDIR"
}
trap cleanup_filebeat EXIT

imbeats_es_shutdown_empty_check() {
	local base
	local response
	local http_status
	local body
	local lines

	base=$(es_base_url)
	response=$(curl --silent --show-error --write-out '\n%{http_code}' \
		"${base}/rsyslog_testbench/_count")
	http_status=$(printf '%s' "$response" | tail -n 1)
	body=$(printf '%s' "$response" | sed '$d')
	if [ "$http_status" = "404" ]; then
		lines=0
	elif [ "$http_status" = "200" ]; then
		lines=$(printf '%s' "$body" | "$PYTHON" -c '
import json
import sys

print(json.load(sys.stdin).get("count", 0))
')
	else
		printf '%s imbeats_es_shutdown_empty_check: unexpected Elasticsearch status %s\n' \
			"$(tb_timestamp)" "$http_status"
		return 1
	fi
	if [ "$lines" -eq "$NUMMESSAGES" ]; then
		printf '%s imbeats_es_shutdown_empty_check: success, have %d lines\n' "$(tb_timestamp)" "$lines"
		return 0
	fi
	printf '%s imbeats_es_shutdown_empty_check: have %d lines, expecting %d\n' \
		"$(tb_timestamp)" "$lines" "$NUMMESSAGES"
	return 1
}

ensure_elasticsearch_ready
init_elasticsearch

mkdir -p "$FB_WORKDIR/data"
for i in $(seq 0 $((NUMMESSAGES - 1))) ; do
	printf 'msgnum:%08d:\n' "$i"
done > "$FB_INPUT"

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

template(name="es_tpl" type="string"
         string="{\"msgnum\":\"%$!message:F,58:2%\"}")

ruleset(name="beats_to_es") {
  action(type="omelasticsearch"
         server="127.0.0.1"
         serverport="'$ES_PORT'"
         template="es_tpl"
         searchIndex="rsyslog_testbench")
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

es_getdata "$NUMMESSAGES" "$ES_PORT"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
