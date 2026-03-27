#!/bin/bash
# Tests reload_lookup_table: statement in a YAML statements: block.
#
# A lookup table is loaded via lookup_tables: and a statements: block
# uses reload_lookup_table: conditional on a specific message.  This
# exercises the YAML → RainerScript code path for the statement, verifying
# the positional arguments ("table"[, "stub_value"]) are emitted correctly.
#
# Messages are injected via imdiag (injectmsg) and routed to the YAML
# "main" ruleset via a "call main" statement added to the default ruleset
# in the .conf preamble.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD" "This test currently does not work on FreeBSD"
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
# Route imdiag-injected messages from DefaultRuleset into main
call main
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
lookup_tables:
  - name: xlate
    file: "${RSYSLOG_DYNNAME}.xlate.lkp_tbl"

templates:
  - name: outfmt
    type: string
    string: "- %msg% %$.lkp%\n"

rulesets:
  - name: main
    statements:
      - set:
          var: "$.lkp"
          expr: 'lookup("xlate", $msg)'
      - if: '$msg contains "msgnum:00000002:"'
        then:
          - reload_lookup_table:
              table: xlate
              stub_value: reload_failed
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        template: outfmt
YAMLEOF
sed -i \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    -e "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

cp -f "$srcdir/testsuites/xlate.lkp_tbl" "${RSYSLOG_DYNNAME}.xlate.lkp_tbl"
startup
# Put updated table in place; message 2 will trigger reload
cp -f "$srcdir/testsuites/xlate_more.lkp_tbl" "${RSYSLOG_DYNNAME}.xlate.lkp_tbl"
injectmsg 0 3
await_lookup_table_reload
wait_queueempty
content_check "msgnum:00000000: foo_old"
content_check "msgnum:00000001: bar_old"
shutdown_when_empty
wait_shutdown
exit_test
