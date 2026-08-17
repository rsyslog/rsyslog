#!/bin/bash
# Verify includeNamespaceMetadata="off" skips the namespace API lookup while
# retaining parsed namespace identity and pod metadata. The output fields and
# absence of a namespace-only request in the test-server log are the oracle.
# The two-minute server timeout is hang protection; readiness and cleanup use
# PID/port files rather than timing assumptions.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available timeout
require_plugin mmkubernetes ../contrib/mmkubernetes

pwd=$(pwd)
testsrv=mmk8s-test-server
k8s_srv_port_file="${RSYSLOG_DYNNAME}${testsrv}.port"
generate_conf

timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py 0 \
	${RSYSLOG_DYNNAME}${testsrv}.pid \
	${RSYSLOG_DYNNAME}${testsrv}.started \
	${k8s_srv_port_file} > ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log 2>&1 &
BGPROCESS=$!
wait_file_exists "$k8s_srv_port_file"
k8s_srv_port="$(cat "$k8s_srv_port_file")"
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imfile/.libs/imfile")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../contrib/mmkubernetes/.libs/mmkubernetes")
input(type="imfile" file="'$RSYSLOG_DYNNAME.spool'/pod-*.log" tag="kubernetes" addmetadata="on")
action(type="mmjsonparse" cookie="")
action(type="mmkubernetes" token="dummy"
       kubernetesurl="http://localhost:'$k8s_srv_port'"
       includeNamespaceMetadata="off"
       filenamerules=["rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"])
template(name="outfmt" type="string" string="%$!all-json-plain%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

cat > ${RSYSLOG_DYNNAME}.spool/pod-name2578_namespace-name2578_container-name2578-id2578.log <<EOF
{"message":"namespace metadata disabled","testid":2578}
EOF

startup
wait_queueempty
shutdown_when_empty
wait_shutdown
kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

$PYTHON - "$RSYSLOG_OUT_LOG" "${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log" <<'PY'
import json
import sys

record = next(item for item in map(json.loads, open(sys.argv[1])) if item.get("testid") == 2578)
kubernetes = record["kubernetes"]
assert kubernetes["namespace_name"] == "namespace-name2578", kubernetes
assert kubernetes["pod_id"] == "pod-name2578-id", kubernetes
for field in ("namespace_id", "namespace_labels", "namespace_annotations", "creation_timestamp"):
    assert field not in kubernetes, (field, kubernetes)
server_log = open(sys.argv[2]).read()
assert "/api/v1/namespaces/namespace-name2578 HTTP" not in server_log, server_log
assert "/api/v1/namespaces/namespace-name2578/pods/pod-name2578 HTTP" in server_log, server_log
PY
if [ $? -ne 0 ]; then
	cat ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
