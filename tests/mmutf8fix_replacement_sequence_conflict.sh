#!/bin/bash
# Validate mmutf8fix configuration rejects replacementChar and
# replacementSequence on the same action. This is a startup validation test:
# rsyslog must refuse the configuration before any normal message path exists.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi

generate_conf
add_conf '
module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
action(type="mmutf8fix" replacementChar="?" replacementSequence="[bad]")
'

export CONF_FILE="${TESTCONF_NM}.conf"
if $RSYSLOGD -N1 -M"$RSYSLOG_MODDIR" -f "$CONF_FILE" > stdout.log 2> stderr.log; then
    echo "FAIL: invalid replacement configuration was accepted"
    error_exit 1
fi

content_check "mmutf8fix: replacementChar and replacementSequence are mutually exclusive" stderr.log
exit_test
