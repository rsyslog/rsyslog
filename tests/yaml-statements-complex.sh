#!/bin/bash
# Tests the YAML-native statements complex routing and iteration example
# from the YAML configuration documentation.
#
# Added 2026, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imptcp/.libs/imptcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"

templates:
  - name: outfmt
    type: string
    string: "val: %$.item!val%\n"

rulesets:
  - name: process_items
    statements:
      - set:
          var: "$.is_audit"
          expr: 're_match($msg, "AUDIT")'
      - type: mmjsonparse
      - foreach:
          var: "$.item"
          in: "$!items"
          do:
            - if: '$.is_audit == 1'
              then:
                - type: omfile
                  file: "${RSYSLOG_OUT_LOG}.audit"
                  template: outfmt
              else:
                - if: '$.item!type == "error"'
                  then:
                    - type: omfile
                      file: "${RSYSLOG_OUT_LOG}.error"
                      template: outfmt
                  else:
                    - type: omfile
                      file: "${RSYSLOG_OUT_LOG}.standard"
                      template: outfmt
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imptcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="process_items")
'
startup
tcpflood -m 1 -I $srcdir/testsuites/yaml_complex_input
shutdown_when_empty
wait_shutdown

# The AUDIT message has "audit1" and "audit2" which both go to the audit log
custom_content_check "val: audit1" "${RSYSLOG_OUT_LOG}.audit"
custom_content_check "val: audit2" "${RSYSLOG_OUT_LOG}.audit"

# The NORMAL message has "err1" (type error) and "std1" (type info)
custom_content_check "val: err1" "${RSYSLOG_OUT_LOG}.error"

custom_content_check "val: std1" "${RSYSLOG_OUT_LOG}.standard"

exit_test
