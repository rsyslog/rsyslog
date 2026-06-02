#!/bin/bash
# check translation warnings for legacy selector syntax.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../plugins/omstdout/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
module(load="omstdout")
*.* :omstdout:
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath"

content_check '# TRANSLATION WARNING: top-level statements normalized into explicit RSYSLOG_DefaultRuleset' "${RSYSLOG_DYNNAME}.yaml"
content_check '# TRANSLATION WARNING: legacy action syntax preserved as script text' "${RSYSLOG_DYNNAME}.yaml"
content_check 'name: "RSYSLOG_DefaultRuleset"' "${RSYSLOG_DYNNAME}.yaml"
content_check '*.* :omstdout:' "${RSYSLOG_DYNNAME}.yaml"

echo SUCCESS: legacy translation warning coverage
exit_test
