#!/bin/bash
# Regression coverage for issue #1129: setting debug globals during -N1 config
# validation must not make the config checker return failure when their setup
# messages are informational.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(
	debug.logFile="'"$RSYSLOG_DYNNAME"'.debug.log"
	debug.onShutdown="on"
)
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

content_check "End of config validation run" "${RSYSLOG_DYNNAME}.rsyslog.log"
check_file_exists "${RSYSLOG_DYNNAME}.debug.log"

exit_test
