#!/bin/bash
# Test startup fallback with a bad classic .qi file. The daemon must survive,
# quarantine the corrupt control file, create a fresh classic DA child, process
# messages, and shut down cleanly. A per-test copy keeps the source fixture
# immutable while exercising the real safe-mode recovery path.
# added 2009-10-21 by RGerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[badqi.sh\]: test startup with invalid .qi file
. ${srcdir:=.}/diag.sh init
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
mkdir -p "$SPOOL_DIR"
cp "$srcdir/bad_qi/dbq.qi" "$SPOOL_DIR/dbq.qi"
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
# instruct to use bad .qi file
$WorkDirectory '"$SPOOL_DIR"'
$ActionQueueType LinkedList
$ActionQueueFileName dbq
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
# we just inject a handful of messages so that we have something to wait for...
tcpflood -m20
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown  # wait for process to terminate
seq_check 0 19
exit_test
