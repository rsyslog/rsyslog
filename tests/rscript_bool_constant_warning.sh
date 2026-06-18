#!/bin/bash
# Verify that a common RainerScript boolean mistake logs a config warning:
# `$msg contains "a" or "b"` keeps historical truthiness semantics, but the
# bare string literal is probably a missing repeated comparison. The oracle is
# rsyslogd config-check output because the warning is emitted while optimizing
# the parsed configuration, before normal message routing is relevant.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
if $msg contains "alpha" or "beta" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
}
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

content_check "boolean operator 'OR' has constant right operand" "${RSYSLOG_DYNNAME}.rsyslog.log"
exit_test
