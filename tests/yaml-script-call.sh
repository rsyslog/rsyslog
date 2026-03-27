#!/bin/bash
# Tests the "call" statement inside a YAML script: block.
# Two rulesets (rs1 and rs2) are defined in the same YAML file.
# Messages arrive on the input bound to rs1; rs1 calls rs2; rs2 writes
# the message to the output file.
# Mirrors rscript_ruleset_call.sh using YAML configuration.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=5000
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
  - name: rs2
    script: |
      action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
      stop
  - name: rs1
    script: |
      if $msg contains "msgnum:" then
        call rs2
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="rs1")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
