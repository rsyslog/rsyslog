#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression for low-year RFC5424 timestamps reaching weekday formatters.
# A negative C remainder previously indexed before wdayNames/one_digit and
# could crash rsyslogd. Exact formatted output and processing of later valid
# messages are the oracle.
# shellcheck disable=SC1091,SC2086
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=6
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="weekday" type="string"
         string="%msg%:%timereported:::date-wdayname%/%timereported:::date-wday%\n")
:msg, contains, "case:" action(type="omfile" template="weekday"
                              file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg_literal "<165>1 0001-01-02T00:00:00Z h a - - - case:low2"
injectmsg_literal "<165>1 0001-01-06T00:00:00Z h a - - - case:low6"
injectmsg_literal "<165>1 0001-01-07T00:00:00Z h a - - - case:low7"
injectmsg_literal "<165>1 1970-01-01T00:00:00Z h a - - - case:epoch"
injectmsg_literal "<165>1 2026-11-13T00:00:00Z h a - - - case:current"
injectmsg_literal "<165>1 2024-02-29T00:00:00Z h a - - - case:leap"
shutdown_when_empty
wait_shutdown
LC_ALL=C sort "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.sorted"
mv "$RSYSLOG_DYNNAME.sorted" "$RSYSLOG_OUT_LOG"
export EXPECTED="case:current:Fri/5
case:epoch:Thu/4
case:leap:Thu/4
case:low2:Mon/1
case:low6:Fri/5
case:low7:Sat/6"
cmp_exact
exit_test
