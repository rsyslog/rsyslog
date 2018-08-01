#!/bin/bash
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[discard-rptdmsg.sh\]: testing discard-rptdmsg functionality
. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun 13514

$RepeatedMsgReduction on

:msg, contains, "00000001" ~

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
. $srcdir/diag.sh tcpflood -m10 -i1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 2 10
exit_test
