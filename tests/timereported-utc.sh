#!/bin/bash
# addd 2016-03-22 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=3
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="outfmt" type="list") {
	property(name="timereported" dateformat="rfc3339" date.inUTC="on")
	constant(value="\n")
}
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg_literal "<165>1 2003-08-24T05:14:15.000003-07:00 192.0.2.1 tcpflood 8710 - - msgnum:0000000"
injectmsg_literal "<165>1 2016-03-01T12:00:00-02:00 192.0.2.1 tcpflood 8710 - - msgnum:0000000"
injectmsg_literal "<165>1 2016-03-01T12:00:00Z 192.0.2.1 tcpflood 8710 - - msgnum:0000000"
shutdown_when_empty
wait_shutdown

export EXPECTED="2003-08-24T12:14:15.000003+00:00
2016-03-01T14:00:00.000000+00:00
2016-03-01T12:00:00.000000+00:00"
cmp_exact
exit_test
