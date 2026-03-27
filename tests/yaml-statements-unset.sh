#!/bin/bash
# Tests the YAML-native statements: unset: and call_indirect: constructs.
# rs1 sets a variable to a ruleset name, then uses call_indirect to dispatch
# to that ruleset.  The target ruleset (rs2) uses unset: to clear a variable
# and then writes matching messages to the output.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=200
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
      - unset: "$.scratch"
      - if: '$msg contains "msgnum:"'
        action:
          type: omfile
          template: outfmt
          file: "${RSYSLOG_OUT_LOG}"
      - stop: true

  - name: rs1
    statements:
      - set:
          var: "$.scratch"
          expr: '"will be unset"'
      - set:
          var: "$.target"
          expr: '"rs2"'
      - call_indirect: "$.target"
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
