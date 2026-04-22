#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imkubernetes ../contrib/imkubernetes
check_command_available timeout

export NUMMESSAGES=2
k8s_srv_port=$( get_free_port )
pwd=$( pwd )
testsrv=imk8s-test-server
logdir="${RSYSLOG_DYNNAME}.spool/pods/namespace-name1_pod-name1_uid1/container-a"
logfile="${logdir}/0.log"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../contrib/imkubernetes/.libs/imkubernetes"
       LogFileGlob="'$pwd/$logdir'/*.log"
       PollingInterval="1"
       KubernetesUrl="http://localhost:'$k8s_srv_port'"
       Token="dummy"
       cacheEntryTtl="60")

template(name="outfmt" type="list" option.jsonf="on") {
    property(outname="msg" name="msg" format="jsonf")
    property(outname="namespace" name="$!kubernetes!namespace_name" format="jsonf")
    property(outname="pod" name="$!kubernetes!pod_name" format="jsonf")
    property(outname="pod_uid" name="$!kubernetes!pod_uid" format="jsonf")
    property(outname="container" name="$!kubernetes!container_name" format="jsonf")
    property(outname="restart_count" name="$!kubernetes!restart_count" format="jsonf" dataType="number")
    property(outname="stream" name="$!kubernetes!stream" format="jsonf")
    property(outname="format" name="$!kubernetes!log_format" format="jsonf")
    property(outname="pod_id" name="$!kubernetes!pod_id" format="jsonf")
    property(outname="pod_ip" name="$!kubernetes!pod_ip" format="jsonf")
    property(outname="host" name="$!kubernetes!host" format="jsonf")
    property(outname="label_component" name="$!kubernetes!labels!component" format="jsonf")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

mkdir -p "$logdir"
echo starting kubernetes test server
MMK8S_INCLUDE_NODE_NAME=1 timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py \
	$k8s_srv_port \
	${RSYSLOG_DYNNAME}${testsrv}.pid \
	${RSYSLOG_DYNNAME}${testsrv}.started \
	> ${RSYSLOG_DYNNAME}.spool/imkubernetes_srv.log 2>&1 &
BGPROCESS=$!
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started

startup

cat > "$logfile" <<'EOF'
2026-04-20T10:00:00.123456789Z stdout F hello from cri
2026-04-20T10:00:01.000000000Z stderr P partial one 
2026-04-20T10:00:02.000000000Z stderr F partial two
EOF

wait_file_lines "$RSYSLOG_OUT_LOG" 3
wait_queueempty
shutdown_when_empty
wait_shutdown

kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

$PYTHON - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

path = sys.argv[1]
records = [json.loads(line) for line in open(path)]
records = [item for item in records if item.get("format") == "cri"]
assert len(records) == 2, records
by_msg = {item["msg"]: item for item in records}

cri = by_msg["hello from cri"]
assert cri["namespace"] == "namespace-name1"
assert cri["pod"] == "pod-name1"
assert cri["pod_uid"] == "uid1"
assert cri["container"] == "container-a"
assert cri["restart_count"] == 0
assert cri["stream"] == "stdout"
assert cri["format"] == "cri"
assert cri["pod_id"] == "pod-name1-id"
assert cri["pod_ip"] == "10.128.0.14"
assert cri["host"] == "pod-name1-node"
assert cri["label_component"] == "pod-name1-component"

partial = by_msg["partial one partial two"]
assert partial["stream"] == "stderr"
assert partial["format"] == "cri"
assert partial["pod_id"] == "pod-name1-id"
PY

exit_test
