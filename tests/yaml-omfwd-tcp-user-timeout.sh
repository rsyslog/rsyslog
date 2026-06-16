#!/bin/bash
# Verify the YAML frontend accepts omfwd tcp_user_timeout and forwards over TCP.
# The receiver output is the oracle so the test covers the configured output
# destination, not rsyslog startup diagnostics.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000
include(file="'${RSYSLOG_DYNNAME}'.yaml")
call main
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    filter: ':msg, contains, "msgnum:"'
    actions:
      - type: omfwd
        template: outfmt
        target: 127.0.0.1
        port: "@TCPFLOOD_PORT@"
        protocol: tcp
        tcp_user_timeout: 10000
YAMLEOF

sed -i "s|@TCPFLOOD_PORT@|${TCPFLOOD_PORT}|g" "${RSYSLOG_DYNNAME}.yaml"

start_minitcpsrv_ready "$RSYSLOG_OUT_LOG" "$TCPFLOOD_PORT"
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

seq_check 0 0
exit_test
