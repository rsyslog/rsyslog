#!/bin/bash
# Verify YAML global.executionEngine reaches the shared backend and enables a
# reserved-batch queued ruleset call. Exact output after synchronized shutdown
# is the oracle; startup alone is not sufficient.
. ${srcdir:=.}/diag.sh init
export RSTB_EXECUTION_ENGINE=reservedBatch
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'${RSYSLOG_DYNNAME}'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: target'
add_yaml_conf '    queue.type: LinkedList'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      if $msg contains "msgnum" then call target'
startup
tcpflood -m 5
shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 5 "$RSYSLOG_OUT_LOG"
exit_test
