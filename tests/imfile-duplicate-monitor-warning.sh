#!/bin/bash
# Verifies that imfile warns only for probably accidental duplicate file
# monitors. The first config-check uses three identical monitors and must emit
# one bounded warning per later duplicate, not every equivalent pair. The
# second config-check uses the same file with distinct tags and a rate-limited
# monitor; it must stay warning-free because this is a valid same-file fan-out
# pattern.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

mkdir "$RSYSLOG_DYNNAME.work"
touch "$RSYSLOG_DYNNAME.input"

generate_conf
add_conf '
global(workDirectory="./'$RSYSLOG_DYNNAME'.work")
module(load="../plugins/imfile/.libs/imfile" mode="polling")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="dup:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="dup:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="dup:")
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.dup.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

content_check "imfile: duplicate file monitor for" "${RSYSLOG_DYNNAME}.dup.log"
content_check "with tag 'dup:' has identical delivery parameters" "${RSYSLOG_DYNNAME}.dup.log"
warning_count=$(grep -c -F "imfile: duplicate file monitor for" "${RSYSLOG_DYNNAME}.dup.log")
if [ "$warning_count" -ne 2 ]; then
	echo "expected two bounded duplicate monitor warnings, got $warning_count"
	cat -n "${RSYSLOG_DYNNAME}.dup.log"
	error_exit 1
fi

generate_conf
add_conf '
global(workDirectory="./'$RSYSLOG_DYNNAME'.work")
module(load="../plugins/imfile/.libs/imfile" mode="polling")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="first:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="second:")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="limited:" maxLinesPerMinute="1")
'

export RS_REDIR=">${RSYSLOG_DYNNAME}.distinct.log 2>&1"
rsyslogd_config_check
unset RS_REDIR

check_not_present "imfile: duplicate file monitor for" "${RSYSLOG_DYNNAME}.distinct.log"

exit_test
