#!/bin/bash
# check translation of traditional file action shorthand into YAML filter/actions.
#
# Many distros still ship defaults like:
#   user.* -/var/log/user.log
# so translation coverage for this legacy form is important.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
user.*				-/var/log/user.log
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?
cat -n "${RSYSLOG_DYNNAME}.yaml"

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<'YAML_EOF'
version: 2

rulesets:
  # TRANSLATION WARNING: top-level statements normalized into explicit RSYSLOG_DefaultRuleset
  - name: "RSYSLOG_DefaultRuleset"
    filter: "user.*"
    actions:
      - type: "omfile"
        file: "/var/log/user.log"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

echo SUCCESS: legacy file-action shorthand translation coverage
exit_test
