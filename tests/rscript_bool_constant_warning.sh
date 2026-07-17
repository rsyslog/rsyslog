#!/bin/bash
# Verify that a common RainerScript boolean mistake logs a config warning:
# `$msg contains "a" or "b"` keeps historical truthiness semantics, but the
# bare string literal is probably a missing repeated comparison. The warning
# must only fire on constants as written: comparisons that merely constant-fold
# to a constant (e.g. backtick-expanded environment variables) are legitimate
# and must stay silent. The oracle is rsyslogd config-check output.
. ${srcdir:=.}/diag.sh init

export MY_FEATURE="on"
generate_conf
add_conf '
if $msg contains "alpha" or "beta" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
}
if `echo $MY_FEATURE` == "on" and $msg contains "gamma" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
}
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

content_check "boolean operator 'OR' has constant right operand" "${RSYSLOG_DYNNAME}.rsyslog.log"
check_not_present "boolean operator 'AND' has constant" "${RSYSLOG_DYNNAME}.rsyslog.log"
exit_test
