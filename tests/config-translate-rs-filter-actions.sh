#!/bin/bash
# check translation of a simple selector/action ruleset into YAML filter/actions.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../.libs"
outlog="/tmp/${RSYSLOG_DYNNAME}.filter-actions.log"

cat > "${RSYSLOG_DYNNAME}.conf" <<RS_EOF
ruleset(name="main") {
  mail.info action(type="omfile" file="${outlog}")
}
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<YAML_EOF
version: 2

rulesets:
  - name: "main"
    filter: "mail.info"
    actions:
      - type: "omfile"
        file: "${outlog}"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?

echo SUCCESS: selector/action to YAML filter/actions translation
exit_test
