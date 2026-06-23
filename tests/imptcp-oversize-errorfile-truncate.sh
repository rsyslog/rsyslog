#!/bin/bash
# Regression test for oversizemsg.errorfile with imptcp truncation.
# imptcp can detect and truncate an oversized TCP frame before core
# submission sees rawmsg > maxMessageSize. The test passes only if that
# pre-submit oversize detection writes the configured JSON errorfile with the
# expanded message-field schema operators need for triage. This path runs
# before parser-derived field values such as msg and syslogtag are known, so
# the oracle checks that those fields exist while expecting imptcp-populated
# fields to carry values. It also proves the legacy "input" alias is preserved
# for consumers of the older two-field record.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on"
	oversizemsg.input.mode="truncate"
	oversizemsg.errorfile="'$RSYSLOG2_OUT_LOG'")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
msg="<120> 2011-03-01T11:22:12Z host tag: this is a way too long message that has to be truncated test1 test2 test3 test4 test5 abcdefghijklmnopqrstuvwxyz"
printf '%s %s' "${#msg}" "$msg" > $RSYSLOG_DYNNAME.inputfile
tcpflood -I $RSYSLOG_DYNNAME.inputfile
shutdown_when_empty
wait_shutdown

content_check --regex '"rawmsg":.*way too long message' "$RSYSLOG2_OUT_LOG"
content_check '"msg":' "$RSYSLOG2_OUT_LOG"
content_check '"inputname": "imptcp"' "$RSYSLOG2_OUT_LOG"
content_check '"input": "imptcp"' "$RSYSLOG2_OUT_LOG"
content_check '"fromhost-ip": "127.0.0.1"' "$RSYSLOG2_OUT_LOG"
content_check '"timereported":' "$RSYSLOG2_OUT_LOG"
content_check '"timegenerated":' "$RSYSLOG2_OUT_LOG"
content_check '"syslogtag":' "$RSYSLOG2_OUT_LOG"

exit_test
