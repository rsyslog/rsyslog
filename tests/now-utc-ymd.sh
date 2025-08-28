#!/bin/bash
# test many concurrent tcp connections
# add 2016-02-23 by RGerhards, released under ASL 2.0
# requires faketime
. ${srcdir:=.}/diag.sh init
echo \[now-utc-ymd\]: test \$year-utc, \$month-utc, \$day-utc

. $srcdir/faketime_common.sh

export TZ=TEST-02:00

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%$year%-%$month%-%$day%,%$year-utc%-%$month-utc%-%$day-utc%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
FAKETIME='2016-01-01 01:00:00' startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="2016-01-01,2015-12-31"
exit_test
