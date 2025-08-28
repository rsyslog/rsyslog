#!/bin/bash
# test $NOW family of system properties
# add 2016-01-12 by RGerhards, released under ASL 2.0
# requires faketime
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%$hour%:%$minute%,%$hour-utc%:%$minute-utc%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'

. $srcdir/faketime_common.sh

export TZ=TEST+06:30

FAKETIME='2016-01-01 01:00:00' startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="01:00,07:30"
cmp_exact

exit_test
