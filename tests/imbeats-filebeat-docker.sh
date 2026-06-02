#!/bin/bash
. ${srcdir:=.}/diag.sh init
check_command_available docker

if ! docker info >/dev/null 2>&1 ; then
	echo "docker daemon not available, skipping test"
	exit 77
fi

export NUMMESSAGES=200
export SEQ_CHECK_FILE="${TESTTOOL_DIR}/${RSYSLOG_OUT_LOG}"
export FB_IMAGE="${FB_IMAGE:-docker.elastic.co/beats/filebeat:9.2.0}"
export FB_CONTAINER="fb-imbeats-${RSYSLOG_DYNNAME}"
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

mkdir -p "$FB_WORKDIR/data"
for i in $(seq 0 $((NUMMESSAGES - 1))) ; do
	printf 'msgnum:%08d:\n' "$i"
done > "$FB_INPUT"

generate_conf
add_conf '
module(load="../plugins/imbeats/.libs/imbeats")

template(name="outfmt" type="string" string="%$!message%\n")

ruleset(name="main") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

input(type="imbeats"
      port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.imbeats.port"
      ruleset="main")
'

startup
assign_rs_port "$RSYSLOG_DYNNAME.imbeats.port"

cat > "$FB_CONFIG" <<EOF
filebeat.inputs:
  - type: filestream
    id: imbeats-test
    enabled: true
    paths:
      - /work/input.log

output.logstash:
  hosts: ["127.0.0.1:${RS_PORT}"]

logging.level: info
logging.to_files: false
EOF

docker image inspect "$FB_IMAGE" >/dev/null 2>&1 || docker pull "$FB_IMAGE" >/dev/null

docker run -d --rm \
	--name "$FB_CONTAINER" \
	--network host \
	--user root \
	-v "$FB_DOCKER_MOUNT_SOURCE:/work" \
	"$FB_IMAGE" \
	filebeat -e --strict.perms=false -c /work/filebeat.yml > "$FB_LOG"

wait_file_lines "$SEQ_CHECK_FILE" "$NUMMESSAGES"

docker rm -f "$FB_CONTAINER" >/dev/null 2>&1 || true

shutdown_when_empty
wait_shutdown

cmp "$FB_INPUT" "$SEQ_CHECK_FILE"
exit_test
