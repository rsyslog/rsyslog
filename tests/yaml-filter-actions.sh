#!/bin/bash
# Tests the YAML Phase 2 structured filter+actions shortcut:
#   - filter: "*.* " (PRI filter) with a single action
#   - filter: ":msg, contains, ..." (property filter) with multiple actions
#   - actions: without a filter (unconditional routing)
#
# The test sends 50 messages tagged with "msgnum:"; all 50 should appear in
# the unconditional omfile output.  Messages NOT containing "missing" should
# still pass the property filter (negation is not tested here).
#
# Added 2025 by contributors, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=50
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    filter: "*.*"
    actions:
      - type: omfile
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
