#!/bin/bash
# Tests the YAML-native statements complex routing and iteration example
# from the YAML configuration documentation.
#
# Added 2026, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
out_audit="${RSYSLOG_OUT_LOG}.audit"
out_error="${RSYSLOG_OUT_LOG}.error"
out_standard="${RSYSLOG_OUT_LOG}.standard"
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
modules:
  - load: "../plugins/imptcp/.libs/imptcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"

templates:
  - name: outfmt
    type: string
    string: "val: %\$.item!val%\n"

rulesets:
  - name: process_items
    statements:
      - set:
          var: "\$.is_audit"
          expr: 're_match(\$msg, "AUDIT")'
      - type: mmjsonparse
      - foreach:
          var: "\$.item"
          in: "\$!items"
          do:
            - if: '\$.is_audit == 1'
              then:
                - type: omfile
                  file: "${out_audit}"
                  template: outfmt
              else:
                - if: '\$.item!type == "error"'
                  then:
                    - type: omfile
                      file: "${out_error}"
                      template: outfmt
                  else:
                    - type: omfile
                      file: "${out_standard}"
                      template: outfmt
YAMLEOF

add_conf '
input(type="imptcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="process_items")
'
startup
tcpflood -p"$(cat "${RSYSLOG_DYNNAME}.tcpflood_port")" -m 1 -I "$srcdir/testsuites/yaml_complex_input"
shutdown_when_empty
wait_shutdown

# The AUDIT message has "audit1" and "audit2" which both go to the audit log
custom_content_check "val: audit1" "${out_audit}"
custom_content_check "val: audit2" "${out_audit}"

# The NORMAL message has "err1" (type error) and "std1" (type info)
custom_content_check "val: err1" "${out_error}"

custom_content_check "val: std1" "${out_standard}"

exit_test
