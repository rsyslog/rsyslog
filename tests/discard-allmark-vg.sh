#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# Under Valgrind, discard message 1 with ActionWriteAllMarkMessages
# enabled. Give the instrumented receiver 1.5 seconds before shutdown.
# Require a clean Valgrind result and output messages 2 through 10.

uname
if [ $(uname) = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

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
startup_vg
tcpflood -m10 -i1
# we need to give rsyslog a little time to settle the receiver
./msleep 1500
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg
check_exit_vg
seq_check 2 10
exit_test
