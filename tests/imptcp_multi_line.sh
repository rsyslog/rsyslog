#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="remote" multiline="on")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -B -I ${srcdir}/testsuites/imptcp_multi_line.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
export EXPECTED='NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test1
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test2
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012line1
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012l#012i#012n#012#012e2
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test3
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012line3
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test4
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test end'
cmp_exact
exit_test
