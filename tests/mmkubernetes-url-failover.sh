#!/bin/bash
# Verify that mmkubernetes can try multiple kubernetesurl values configured as
# a standard rsyslog array. The first URL is an unreachable localhost endpoint;
# the oracle is that metadata enrichment still succeeds through the second,
# live test server.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available timeout
pwd=$( pwd )
k8s_srv_port_file="${RSYSLOG_DYNNAME}mmk8s-test-server.port"
generate_conf
testsrv=mmk8s-test-server

echo starting kubernetes \"emulator\"
timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py 0 \
	${RSYSLOG_DYNNAME}${testsrv}.pid \
	${RSYSLOG_DYNNAME}${testsrv}.started \
	${k8s_srv_port_file} > ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log 2>&1 &
BGPROCESS=$!
wait_file_exists "$k8s_srv_port_file"
k8s_srv_port="$(cat "$k8s_srv_port_file")"
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started
echo background mmkubernetes_test_server.py process id is $BGPROCESS

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imfile/.libs/imfile")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../contrib/mmkubernetes/.libs/mmkubernetes")

template(name="mmk8s_template" type="list") {
    property(name="$!all-json-plain")
    constant(value="\n")
}

input(type="imfile" file="'$RSYSLOG_DYNNAME.spool'/pod-*.log" tag="kubernetes" addmetadata="on")
action(type="mmjsonparse" cookie="")
action(type="mmkubernetes" token="dummy"
       kubernetesurl=["http://127.0.0.1:1", "http://localhost:'$k8s_srv_port'"]
       filenamerules=["rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"]
)
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="mmk8s_template")
'

cat > ${RSYSLOG_DYNNAME}.spool/pod-name2581_namespace-name2581_container-name2581-id2581.log <<EOF
{"message":"a message for kubernetesurl failover","testid":2581}
EOF

startup
wait_queueempty
shutdown_when_empty
wait_shutdown
kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

$PYTHON - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

records = [json.loads(line) for line in open(sys.argv[1])]
record = next((item for item in records if item.get("testid") == 2581), None)
if record is None:
    print("record for testid 2581 not found")
    sys.exit(1)
kubernetes = record.get("kubernetes", {})
expected = {
    "namespace_name": "namespace-name2581",
    "pod_name": "pod-name2581",
    "container_name": "container-name2581",
    "namespace_id": "namespace-name2581-id",
    "pod_id": "pod-name2581-id",
    "master_url": "http://127.0.0.1:1",
}
for key, value in expected.items():
    if kubernetes.get(key) != value:
        print("unexpected kubernetes.{0}: {1!r} != {2!r}".format(key, kubernetes.get(key), value))
        sys.exit(1)
PY
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected failover-enriched record not found. $RSYSLOG_OUT_LOG is:"
	cat ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi

exit_test
