#!/bin/bash
# test many concurrent tcp connections
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
skip_platform "SunOS"  "timing on connection establishment is different on solaris and makes this test fail"
export NUMMESSAGES=40000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$MaxOpenFiles 2100
module(load="../plugins/imtcp/.libs/imtcp" maxSessions="2100")
input(type="imtcp" socketBacklog="2000" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
# we first send a single message so that the output file is opened. Otherwise, we may
# use up all file handles for tcp connections and so the output file could eventually
# not be opened. 2019-03-18 rgerhards
tcpflood -m1
tcpflood -c-2000 -i1 -m $((NUMMESSAGES - 1 ))
shutdown_when_empty
wait_shutdown
seq_check
exit_test
