#!/bin/bash
# check if valgrind violations occur. Correct output is not checked.
# added 2011-03-01 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
if [ $(uname) = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
#   exit 77
fi

export NUMMESSAGES=4
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	workerthreads="1") # single worker to ensure proer timing for this test
$RepeatedMsgReduction on

$template outfmt,"%msg:F,58:2%\n"
*.*       action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup_vg
tcpflood -t 127.0.0.1 -m 4 -r -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...\""
tcpflood -t 127.0.0.1 -m 1 -r -M "\"<133>2011-03-01T11:22:12Z host tag msgh ...x\""
shutdown_when_empty
wait_shutdown_vg
check_exit_vg
exit_test
