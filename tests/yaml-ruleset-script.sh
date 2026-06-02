#!/bin/bash
# Tests a ruleset with an inline RainerScript script: block in YAML.
# The script uses an if/then filter expression and a stop statement,
# verifying that cnfAddConfigBuffer() correctly re-enters the RainerScript
# lex/parse pipeline from within the YAML loader.
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
    script: |
      if \$msg contains "msgnum:" then {
        action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
      } else {
        stop
      }
      stop
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
