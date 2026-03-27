#!/bin/bash
# Tests local ($.var) variables inside a YAML script: block combined with
# a complex if/elseif/else chain.
# Sends 2000 messages starting at sequence 1; messages with sequence number
# < 100 or > 999 are dropped; messages 100-999 reach the output file.
# Mirrors stop-localvar.sh using YAML configuration.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=900
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
    string: "%.nbr%\n"

rulesets:
  - name: main
    script: |
      if $msg contains "msgnum:" then {
        set $.nbr = field($msg, 58, 2);
        if cnum($.nbr) < 100 then
          stop
        else if not (cnum($.nbr) > 999) then
          action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
      }
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m2000 -i1
shutdown_when_empty
wait_shutdown
seq_check 100 999
exit_test
