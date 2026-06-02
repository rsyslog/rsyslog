#!/bin/bash
# Tests YAML subtree template: a template of type=subtree produces a JSON
# rendering of the specified message-variable subtree.
#
# The config sets two fields on $!usr, then applies a subtree template
# pointing at $!usr.  The output is valid JSON containing both fields.
# This verifies that the YAML "type: subtree" + "subtree:" parameters
# are wired through to tplProcessCnf() correctly.
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
  # String template used only for seq_check
  - name: seqfmt
    type: string
    string: "%msg:F,58:2%\n"

  # Subtree template: renders $!usr as JSON
  - name: jsontree
    type: subtree
    subtree: "$!usr"

rulesets:
  - name: main
    script: |
      if $msg contains "msgnum:" then {
        set $!usr!msgnum = field($msg, 58, 2);
        set $!usr!host = $hostname;
        action(type="omfile" template="seqfmt" file="${RSYSLOG_OUT_LOG}")
        action(type="omfile" template="jsontree" file="${RSYSLOG_DYNNAME}.json")
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
seq_check
# Each JSON line must contain "msgnum" and "host" keys
if ! grep -q '"msgnum"' "${RSYSLOG_DYNNAME}.json"; then
    echo "FAIL: expected 'msgnum' key in subtree JSON output"
    exit 1
fi
if ! grep -q '"host"' "${RSYSLOG_DYNNAME}.json"; then
    echo "FAIL: expected 'host' key in subtree JSON output"
    exit 1
fi
exit_test
