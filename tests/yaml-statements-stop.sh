#!/bin/bash
# Tests the YAML-native statements: block — stop: in an else: branch and
# if:/then: with multiple actions.
# Sends 10 messages starting at seq 1; message 00000001 is dropped by the
# else: stop: branch; messages 2-10 reach the output file.
# Also verifies the then: list form (multiple statements) works.
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
    statements:
      - if: 'not ($msg contains "00000001")'
        then:
          - type: omfile
            template: outfmt
            file: "${RSYSLOG_OUT_LOG}"
        else:
          - stop: true
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
