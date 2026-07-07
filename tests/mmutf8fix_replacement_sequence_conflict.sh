#!/bin/bash
# Validate mmutf8fix configuration rejects replacementChar and
# replacementSequence on the same action. This is a startup validation test:
# rsyslog must refuse the configuration before any normal message path exists.
# The oracle is the startup diagnostic on stderr, captured in a test-instance
# specific file so parallel negative config tests cannot overwrite it.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi

STDOUT_LOG="${RSYSLOG_DYNNAME}.stdout.log"
STDERR_LOG="${RSYSLOG_DYNNAME}.stderr.log"

generate_conf
add_conf '
module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
action(type="mmutf8fix" replacementChar="?" replacementSequence="[bad]")
'

export CONF_FILE="${TESTCONF_NM}.conf"
if $RSYSLOGD -N1 -M"$RSYSLOG_MODDIR" -f "$CONF_FILE" > "$STDOUT_LOG" 2> "$STDERR_LOG"; then
    echo "FAIL: invalid replacement configuration was accepted"
    error_exit 1
fi

content_check "mmutf8fix: replacementChar and replacementSequence are mutually exclusive" "$STDERR_LOG"
exit_test
