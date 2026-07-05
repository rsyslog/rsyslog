#!/bin/bash
# check that array literal element types (string vs. number) survive
# RainerScript -> YAML -> RainerScript config translation.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../.libs"
target_out="/tmp/${RSYSLOG_DYNNAME}.target.log"

cat > "${RSYSLOG_DYNNAME}.conf" <<RS_EOF
ruleset(name="main") {
  set \$!ids = [1, 2, 3];
  set \$!neg = [-1, -42];
  set \$!tags = ["error", "critical"];
  set \$!mixed = ["level", 42];
  action(type="omfile" file="${target_out}")
}
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.roundtrip.yaml" -M"$modpath" ||
	error_exit $?
../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.roundtrip.yaml" -F rainerscript -o "${RSYSLOG_DYNNAME}.roundtrip.conf" \
	-M"$modpath" || error_exit $?

# numeric elements must stay unquoted through both translation steps
cat > "${RSYSLOG_DYNNAME}.expected.conf" <<RS_EOF
ruleset(name="main") {
  set \$!ids = [1, 2, 3];
  set \$!neg = [-1, -42];
  set \$!tags = ["error", "critical"];
  set \$!mixed = ["level", 42];
  action(type="omfile" file="${target_out}")
}

RS_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.conf" "${RSYSLOG_DYNNAME}.roundtrip.conf"

# the translated config must itself validate
../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.roundtrip.conf" -M"$modpath" || error_exit $?

echo SUCCESS: array literal types survive translation round-trip
exit_test
