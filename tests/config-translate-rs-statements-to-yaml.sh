#!/bin/bash
# Check RainerScript selector bodies translated into canonical YAML statements.
#
# This keeps coverage focused on runtime/translate.c without starting a daemon:
# multiple legacy selector statements must become a YAML statements: list, a
# selector with two structured actions must use then:, and a single-action
# selector must use the compact action: form. The generated YAML is validated
# with -N1 so the exact-output oracle also proves the translation is parseable.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../.libs"
out_a="/tmp/${RSYSLOG_DYNNAME}.a.log"
out_a2="/tmp/${RSYSLOG_DYNNAME}.a2.log"
out_b="/tmp/${RSYSLOG_DYNNAME}.b.log"

cat > "${RSYSLOG_DYNNAME}.conf" <<RS_EOF
ruleset(name="main") {
  mail.info action(type="omfile" file="${out_a}")
  & action(type="omfile" file="${out_a2}")
  authpriv.* action(type="omfile" file="${out_b}")
}
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" ||
	error_exit $?

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<YAML_EOF
version: 2

rulesets:
  - name: "main"
    statements:
      - if: "prifilt('mail.info')"
        then:
          - type: "omfile"
            file: "${out_a}"
          - type: "omfile"
            file: "${out_a2}"
      - if: "prifilt('authpriv.*')"
        action:
          type: "omfile"
          file: "${out_b}"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?

echo SUCCESS: RainerScript selector statements to YAML translation
exit_test
