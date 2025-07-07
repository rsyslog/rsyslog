#!/bin/bash
# the goal here is to detect memleaks when structured data is not
# correctly parsed.
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2015-04-30
. ${srcdir:=.}/diag.sh init
#skip_platform "FreeBSD" "This test currently does not work on FreeBSD."
export USE_VALGRIND="YES" # this test only makes sense with valgrind enabled
# uncomment below to set special valgrind options
#export RS_TEST_VALGRIND_EXTRA_OPTS="--leak-check=full --show-leak-kinds=all"

generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")
module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="mmpstrucdata")
if $msg contains "msgnum" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
# we use different message counts as this hopefully aids us
# in finding which sample is leaking. For this, check the number
# of blocks lost and see what set they match.
tcpflood -m100 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM] invalid structured data!\""
tcpflood -m200 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM ] invalid structured data!\""
tcpflood -m300 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 MSGNUM= ] invalid structured data!\""
tcpflood -m400 -M "\"<161>1 2003-03-01T01:00:00.000Z mymachine.example.com tcpflood - tag [tcpflood@32473 = ] invalid structured data!\""
shutdown_when_empty
wait_shutdown
exit_test
