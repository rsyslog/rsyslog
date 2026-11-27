#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# With ActionWriteAllMarkMessages enabled, discard message 1 before the
# dynamic file action. After shutdown, only messages 2 through 10 may
# appear in the file.
echo ===============================================================================
echo \[discard-allmark.sh\]: testing discard-allmark functionality
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$ActionWriteAllMarkMessages on

:msg, contains, "00000001" ~

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
tcpflood -m10 -i1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 2 10
exit_test
