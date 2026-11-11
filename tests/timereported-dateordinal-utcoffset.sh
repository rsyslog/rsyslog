#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test for the "date-ordinal" property option with timestamps
# that carry a UTC offset. The ordinal must be the day of the year of the
# date as written in the timestamp, independent of offset sign and size.
# Before the fix, getOrdinal() mishandled positive offsets: the result was
# off by up to two days, and early on 1 January with an offset above +12:00
# it became negative, so the daemon could crash while rendering the template.
# In addition, the last second of each day (23:59:59) was counted as the
# following day, which pushed 31 December of a leap year past day 366.
# Oracle: one line per injected message with the exact expected ordinal; a
# crash or wrong value makes cmp_exact fail.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="outfmt" type="string" string="%timereported:::date-ordinal%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg_literal "<165>1 2026-01-01T00:00:00+13:00 192.0.2.1 tcpflood 8710 - - msgnum:0000000"
injectmsg_literal "<165>1 2026-01-01T11:00:00+23:59 192.0.2.1 tcpflood 8710 - - msgnum:0000001"
injectmsg_literal "<165>1 2026-01-02T00:00:00+14:00 192.0.2.1 tcpflood 8710 - - msgnum:0000002"
injectmsg_literal "<165>1 2026-03-01T00:00:00+05:00 192.0.2.1 tcpflood 8710 - - msgnum:0000003"
injectmsg_literal "<165>1 2024-12-31T00:00:00+12:30 192.0.2.1 tcpflood 8710 - - msgnum:0000004"
injectmsg_literal "<165>1 2026-01-01T00:00:00-11:00 192.0.2.1 tcpflood 8710 - - msgnum:0000005"
injectmsg_literal "<165>1 2026-12-31T23:59:59-23:00 192.0.2.1 tcpflood 8710 - - msgnum:0000006"
injectmsg_literal "<165>1 2026-07-15T12:00:00Z 192.0.2.1 tcpflood 8710 - - msgnum:0000007"
injectmsg_literal "<165>1 2024-12-31T23:59:59Z 192.0.2.1 tcpflood 8710 - - msgnum:0000008"
injectmsg_literal "<165>1 2026-03-01T23:59:59Z 192.0.2.1 tcpflood 8710 - - msgnum:0000009"
shutdown_when_empty
wait_shutdown

export EXPECTED="001
001
002
060
366
001
365
196
366
060"
cmp_exact
exit_test
