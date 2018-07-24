#!/bin/bash
# Test for the getenv() rainerscript function
# this is a quick test, but it gurantees that the code path is
# at least progressed (but we do not check for unset envvars!)
# added 2009-11-03 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[rsf_getenv.sh\]: testing RainerScript getenv\(\) function
export MSGNUM="msgnum:"
. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
$InputTCPServerRun 13514

# set spool locations and switch queue to disk-only mode
$WorkDirectory test-spool
$MainMsgQueueFilename mainq
$MainMsgQueueType disk

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
if $msg contains getenv('MSGNUM') then ?dynfile;outfmt
'
startup
. $srcdir/diag.sh tcpflood -m10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 9999
unset MSGNUM
exit_test
