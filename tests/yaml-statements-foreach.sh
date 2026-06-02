#!/bin/bash
# Tests the YAML-native statements: foreach: construct.
# Sends a message containing a JSON array, parses it with mmjsonparse, then
# uses foreach: to iterate and write each element to the output log.
# Mirrors the basic part of json_array_looping.sh using YAML config.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imptcp
require_plugin mmjsonparse
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imptcp/.libs/imptcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"

templates:
  - name: quux
    type: string
    string: "quux: %$.quux%\n"

rulesets:
  - name: freach
    statements:
      - type: mmjsonparse
      - foreach:
          var: "$.quux"
          in: "$!foo"
          do:
            - type: omfile
              file: "${RSYSLOG_OUT_LOG}"
              template: quux
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imptcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="freach")
'
startup
tcpflood -m 1 -I $srcdir/testsuites/json_array_input
shutdown_when_empty
wait_shutdown
content_check 'quux: abc0'
content_check 'quux: def1'
content_check 'quux: ghi2'
exit_test
