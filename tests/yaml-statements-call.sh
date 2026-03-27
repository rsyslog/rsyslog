#!/bin/bash
# Tests the YAML-native statements: block — call: between two rulesets and
# the set: variable assignment statement.
# rs1 sets a local variable, then calls rs2; rs2 uses the variable in its
# own if: condition and writes matching messages to the output.
# Mirrors yaml-script-call.sh using statements: instead of script:.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=500
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
    statements:
      - if: '$msg contains "msgnum:" and $.tag == "seen"'
        action:
          type: omfile
          template: outfmt
          file: "${RSYSLOG_OUT_LOG}"
      - stop: true

  - name: rs1
    statements:
      - set:
          var: "$.tag"
          expr: '"seen"'
      - call: rs2
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
