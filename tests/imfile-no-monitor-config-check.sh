#!/bin/bash
# added 2026-05-22 by Codex, released under ASL 2.0
# Regression coverage for issue #2752: loading imfile without file monitors is
# a warning during config validation, not a hard -N1 failure. The oracle is the
# successful config-check exit code plus the retained warning text.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

content_check "imfile: no files configured to be monitored - no input will be gathered" \
	"${RSYSLOG_DYNNAME}.rsyslog.log"

exit_test
