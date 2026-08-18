#!/bin/bash
# Verify YAML delivers tokenreloadinterval=0 to mmkubernetes while exercising
# the reactive ServiceAccount token reload. The first record starts the worker
# with token v1; after the token file rotates, the second cache miss must reload
# v2 on HTTP 401 and be enriched. The second record's API-derived pod ID is the
# oracle: the server accepts it only after mmkubernetes re-reads token v2. The
# two-minute server timeout is hang protection; PID/port files provide readiness
# and cleanup without sleeps.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available timeout
require_yaml_support
require_plugin mmkubernetes ../contrib/mmkubernetes

pwd=$(pwd)
k8s_srv_port_file="${RSYSLOG_DYNNAME}mmk8s-test-server.port"
token_file=$RSYSLOG_DYNNAME.spool/sa-token
testsrv=mmk8s-test-server

generate_conf --yaml-only
printf 'token-v1' > "$token_file"

# The server compares each bearer token with token_file and returns 401 for the
# stale v1 value, making the reload-and-retry path observable.
MMK8S_EXPECTED_TOKEN_FILE=$pwd/$token_file \
	timeout 2m "$PYTHON" -u "$srcdir/mmkubernetes_test_server.py" 0 \
	"${RSYSLOG_DYNNAME}${testsrv}.pid" \
	"${RSYSLOG_DYNNAME}${testsrv}.started" \
	"${k8s_srv_port_file}" > "${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log" 2>&1 &
BGPROCESS=$!
wait_file_exists "$k8s_srv_port_file"
k8s_srv_port="$(cat "$k8s_srv_port_file")"
wait_process_startup "${RSYSLOG_DYNNAME}${testsrv}" "${RSYSLOG_DYNNAME}${testsrv}.started"

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
add_yaml_conf '    string: "%$!all-json-plain%\\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    statements:'
add_yaml_conf '      - type: mmjsonparse'
add_yaml_conf '        cookie: ""'
add_yaml_conf '      - type: mmkubernetes'
add_yaml_conf '        busyretryinterval: 1'
add_yaml_conf '        tokenfile: "'$pwd/$token_file'"'
add_yaml_conf '        tokenreloadinterval: 0'
add_yaml_conf '        kubernetesurl: "http://localhost:'$k8s_srv_port'"'
add_yaml_conf '        filenamerules:'
add_yaml_conf '          - "rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:.%.%container_hash:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"'
add_yaml_conf '          - "rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
add_yaml_conf '        template: outfmt'

startup

# Record 1 populates the worker's Authorization header with token v1.
cat > "${RSYSLOG_DYNNAME}.spool/pod-name1_namespace-name1_container-name1-id1.log" <<EOF
{"log":"{\"message\":\"HEAD1 / 200 1ms - 9.0B\"}\\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z","testid":1}
EOF
wait_queueempty

# Rotate the projected token. With tokenreloadinterval=0, the following cache
# miss must use the reactive 401 path rather than a periodic reload.
printf 'token-v2' > "$token_file"
cat > "${RSYSLOG_DYNNAME}.spool/pod-name2_namespace-name2_container-name2-id2.log" <<EOF
{"log":"{\"message\":\"HEAD2 / 200 1ms - 9.0B\"}\\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z","testid":2}
EOF
wait_queueempty

shutdown_when_empty
wait_shutdown
kill "$BGPROCESS"
wait_pid_termination "${RSYSLOG_DYNNAME}${testsrv}.pid"

# The second ID can only come from the API after it accepts reloaded token v2.
content_check 'pod-name1-id'
content_check 'pod-name2-id'

exit_test
