#!/bin/bash
# check translation warnings for traditional file action shorthand.
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
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
user.*				-/var/log/user.log
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath" || error_exit $?

content_check '# TRANSLATION WARNING: top-level statements normalized into explicit RSYSLOG_DefaultRuleset' "${RSYSLOG_DYNNAME}.yaml"
content_check '# TRANSLATION WARNING: legacy action syntax preserved as script text' "${RSYSLOG_DYNNAME}.yaml"
content_check 'name: "RSYSLOG_DefaultRuleset"' "${RSYSLOG_DYNNAME}.yaml"
content_check 'user.* -/var/log/user.log' "${RSYSLOG_DYNNAME}.yaml"

echo SUCCESS: legacy file-action shorthand translation coverage
exit_test
