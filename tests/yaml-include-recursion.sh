#!/bin/bash
# Regression test: YAML include recursion must be detected and ignored.
# Setup: root YAML includes itself and also defines a ruleset that writes to
# RSYSLOG_OUT_LOG. Oracle: rsyslog starts and processes messages (no crash /
# stack exhaustion), proving the self-include was ignored rather than recursed.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=20
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
include:
  - path: "${RSYSLOG_DYNNAME}.yaml"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\\n"

rulesets:
  - name: main
    script: |
      :msg, contains, "msgnum:" action(type="omfile"
          template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF

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
