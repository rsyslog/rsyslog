#!/bin/bash
# Tests the "stop" statement inside a YAML script: block.
# Sends 10 messages starting at sequence 1; the script stops the very first
# message (00000001) and lets messages 2-10 through.
# Mirrors stop.sh using YAML configuration.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=9
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    script: |
      if $msg contains "00000001" then
        stop
      if $msg contains "msgnum:" then
        action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m10 -i1
shutdown_when_empty
wait_shutdown
seq_check 2 10
exit_test
