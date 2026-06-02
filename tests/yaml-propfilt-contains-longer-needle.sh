#!/bin/bash
# Exercise the YAML property-filter path with a contains-pattern that is longer
# than the incoming message to ensure the backend returns "no match" cleanly.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
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
    string: "%msg%\n"

rulesets:
  - name: main
    script: |
      if $msg contains "this-needle-is-clearly-longer-than-the-message-under-test" then {
        action(type="omfile" template="outfmt" file="${UNEXPECTED_LOG}")
      }
      action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"
sed -i "s|\${UNEXPECTED_LOG}|${RSYSLOG_DYNNAME}.unexpected.log|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m1 -M "\"<165>1 2003-03-01T01:00:00.000Z host app - - - short\""
shutdown_when_empty
wait_shutdown
if test -e "${RSYSLOG_DYNNAME}.unexpected.log"; then
        test ! -s "${RSYSLOG_DYNNAME}.unexpected.log"
fi
export EXPECTED="short"
cmp_exact
exit_test
