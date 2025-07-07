#!/bin/bash
# test many concurrent tcp connections
# addd 2016-02-23 by RGerhards, released under ASL 2.0
# requires faketime
echo \[timegenerated-ymd\]: check customized format \(Y-m-d\)
. ${srcdir:=.}/diag.sh init

. $srcdir/faketime_common.sh

export TZ=TEST-02:00

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%timegenerated:::date-year%-%timegenerated:::date-month%-%timegenerated:::date-day%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
FAKETIME='2016-01-01 01:00:00' startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="2016-01-01"
cmp_exact


exit_test
