#!/bin/bash
# Tests a more complex inline RainerScript script: block in YAML:
#   - set $!nbr message variable from a field() extraction
#   - nested if expression using cnum() and arithmetic comparison
#   - two sequential actions (unconditional + conditional)
# Sends 500 messages; all pass the outer if (contain "msgnum:"), but only
# messages 0-199 reach the second action (the rest are stopped early to keep
# the test fast).  seq_check 0 499 verifies all 500 reach the first action.
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
  - name: varfmt
    type: string
    string: "%$!nbr%\n"

rulesets:
  - name: main
    script: |
      if $msg contains "msgnum:" then {
        action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
        set $!nbr = field($msg, 58, 2);
        if cnum($!nbr) >= 200 then
          stop
        action(type="omfile" template="varfmt" file="${RSYSLOG_DYNNAME}.detail")
      }
YAMLEOF
sed -i \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    -e "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check 0 499
# The detail file should contain exactly the first 200 messages (0-199)
SEQ_CHECK_FILE="${RSYSLOG_DYNNAME}.detail" seq_check 0 199
exit_test
