#!/bin/bash
# Tests the YAML-native statements: block — basic if:/action: form.
# Demonstrates clean separation: the filter condition is a RainerScript
# expression string, but the action is expressed as a YAML mapping.
# Equivalent to yaml-basic.sh but using statements: instead of script:.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
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
    statements:
      - if: '$msg contains "msgnum:"'
        action:
          type: omfile
          template: outfmt
          file: "${RSYSLOG_OUT_LOG}"
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
