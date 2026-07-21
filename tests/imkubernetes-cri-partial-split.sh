#!/bin/bash
# Verify that CRI partial assembly honors oversizemsg.input.mode="split".
# The partial accumulator must not truncate at maxMessageSize; instead the
# completed logical record reaches the core submit path and is split there.
# The API helper signals readiness after binding; its 2m timeout is hang
# protection, while the explicit kill and wait below own normal cleanup.
. ${srcdir:=.}/diag.sh init
require_plugin imkubernetes ../contrib/imkubernetes
check_command_available timeout

export NUMMESSAGES=3
pwd=$( pwd )
testsrv=imk8s-partial-split-server
k8s_srv_port=0
k8s_srv_port_file=${RSYSLOG_DYNNAME}${testsrv}.port
logdir="${RSYSLOG_DYNNAME}.spool/pods/namespace-name1_pod-name1_uid1/container-a"
logfile="${logdir}/0.log"

mkdir -p "$logdir"
echo starting kubernetes test server
MMK8S_INCLUDE_NODE_NAME=1 timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py \
	0 \
	${RSYSLOG_DYNNAME}${testsrv}.pid \
	${RSYSLOG_DYNNAME}${testsrv}.started \
	$k8s_srv_port_file \
	> ${RSYSLOG_DYNNAME}.spool/imkubernetes_partial_split_srv.log 2>&1 &
BGPROCESS=$!
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started
assign_file_content k8s_srv_port "$k8s_srv_port_file"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'"
       maxMessageSize="128"
       oversizemsg.input.mode="split"
       oversizemsg.report="off")
module(load="../contrib/imkubernetes/.libs/imkubernetes"
       LogFileGlob="'$pwd/$logdir'/*.log"
       PollingInterval="1"
       KubernetesUrl="http://localhost:'$k8s_srv_port'"
       Token="dummy"
       cacheEntryTtl="60")

template(name="outfmt" type="list" option.jsonf="on") {
    property(outname="msg" name="msg" format="jsonf")
    property(outname="stream" name="$!kubernetes!stream" format="jsonf")
    property(outname="format" name="$!kubernetes!log_format" format="jsonf")
    property(outname="pod_id" name="$!kubernetes!pod_id" format="jsonf")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup

{
	for i in $(seq 1 8); do
		printf '2026-04-20T10:00:%02d.000000000Z stdout P %s\n' "$i" \
			'partial-fragment-0123456789'
	done
	printf '%s\n' '2026-04-20T10:00:30.000000000Z stdout F closing-tail-split'
	printf '%s\n' '2026-04-20T10:00:31.000000000Z stdout F after split partial'
} > "$logfile"

wait_file_lines "$RSYSLOG_OUT_LOG" 3
wait_queueempty
shutdown_when_empty
wait_shutdown

kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

if ! $PYTHON - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

path = sys.argv[1]
records = [json.loads(line) for line in open(path)]
records = [item for item in records if item.get("format") == "cri"]
assert len(records) == 3, records

expected = ("partial-fragment-0123456789" * 8) + "closing-tail-split"
first = records[0]
second = records[1]
assert first["msg"] == expected[:128], first
assert second["msg"] == expected[128:], second
for item in (first, second):
    assert item["stream"] == "stdout", item
    assert item["pod_id"] == "pod-name1-id", item

independent = records[2]
assert independent["msg"] == "after split partial", independent
assert independent["stream"] == "stdout", independent
assert independent["pod_id"] == "pod-name1-id", independent
PY
then
	error_exit 1 "imkubernetes did not pass oversized CRI partials to core split handling"
fi

exit_test
