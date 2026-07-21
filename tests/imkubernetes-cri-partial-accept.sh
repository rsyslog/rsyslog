#!/bin/bash
# Verify that CRI partial assembly honors oversizemsg.input.mode="accept".
# A properly closed P...F record that is larger than maxMessageSize but below
# the imkubernetes partial hard cap must reach the core submit path intact.
# The API helper signals readiness after binding; its 2m timeout is hang
# protection, while the explicit kill and wait below own normal cleanup.
. ${srcdir:=.}/diag.sh init
require_plugin imkubernetes ../contrib/imkubernetes
check_command_available timeout

export NUMMESSAGES=2
pwd=$( pwd )
testsrv=imk8s-partial-accept-server
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
	> ${RSYSLOG_DYNNAME}.spool/imkubernetes_partial_accept_srv.log 2>&1 &
BGPROCESS=$!
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started
assign_file_content k8s_srv_port "$k8s_srv_port_file"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'"
       maxMessageSize="128"
       oversizemsg.input.mode="accept")
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
	printf '%s\n' '2026-04-20T10:00:30.000000000Z stdout F closing-tail-kept'
	printf '%s\n' '2026-04-20T10:00:31.000000000Z stdout F after accepted partial'
} > "$logfile"

wait_file_lines "$RSYSLOG_OUT_LOG" 2
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
assert len(records) == 2, records

expected = ("partial-fragment-0123456789" * 8) + "closing-tail-kept"
partial = records[0]
assert partial["msg"] == expected, partial
assert len(partial["msg"]) > 128, partial
assert partial["stream"] == "stdout", partial
assert partial["pod_id"] == "pod-name1-id", partial

independent = records[1]
assert independent["msg"] == "after accepted partial", independent
assert independent["stream"] == "stdout", independent
assert independent["pod_id"] == "pod-name1-id", independent
PY
then
	error_exit 1 "imkubernetes truncated CRI partial despite oversizemsg.input.mode=accept"
fi

exit_test
