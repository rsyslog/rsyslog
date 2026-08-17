#!/bin/bash
# Verify includeNamespaceMetadata=off reaches mmkubernetes through the YAML
# frontend. The retained pod metadata and absent namespace metadata are the
# observable parity oracle. The two-minute server timeout is hang protection;
# readiness and cleanup use PID/port files rather than timing assumptions.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available timeout
require_yaml_support
require_plugin mmkubernetes ../contrib/mmkubernetes

pwd=$(pwd)
testsrv=mmk8s-test-server
k8s_srv_port_file="${RSYSLOG_DYNNAME}${testsrv}.port"
generate_conf --yaml-only

timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py 0 \
	${RSYSLOG_DYNNAME}${testsrv}.pid \
	${RSYSLOG_DYNNAME}${testsrv}.started \
	${k8s_srv_port_file} > ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log 2>&1 &
BGPROCESS=$!
wait_file_exists "$k8s_srv_port_file"
k8s_srv_port="$(cat "$k8s_srv_port_file")"
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started

add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imfile/.libs/imfile"'
add_yaml_conf '  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"'
add_yaml_conf '  - load: "../contrib/mmkubernetes/.libs/mmkubernetes"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imfile'
add_yaml_conf '    file: "'$RSYSLOG_DYNNAME.spool'/pod-*.log"'
add_yaml_conf '    tag: kubernetes'
add_yaml_conf '    addmetadata: on'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%$!all-json-plain%\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    statements:'
add_yaml_conf '      - type: mmjsonparse'
add_yaml_conf '        cookie: ""'
add_yaml_conf '      - type: mmkubernetes'
add_yaml_conf '        token: dummy'
add_yaml_conf '        kubernetesurl: "http://localhost:'$k8s_srv_port'"'
add_yaml_conf '        includenamespacemetadata: off'
add_yaml_conf '        filenamerules:'
add_yaml_conf '          - "rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
add_yaml_conf '        template: outfmt'

cat > ${RSYSLOG_DYNNAME}.spool/pod-name2578_namespace-name2578_container-name2578-id2578.log <<EOF
{"message":"namespace metadata disabled via yaml","testid":2578}
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

record = next(item for item in map(json.loads, open(sys.argv[1])) if item.get("testid") == 2578)
kubernetes = record["kubernetes"]
assert kubernetes["namespace_name"] == "namespace-name2578", kubernetes
assert kubernetes["pod_id"] == "pod-name2578-id", kubernetes
for field in ("namespace_id", "namespace_labels", "namespace_annotations", "creation_timestamp"):
    assert field not in kubernetes, (field, kubernetes)
PY
if [ $? -ne 0 ]; then
	cat ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log
	cat "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
