#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris."
   exit 77
fi

. $srcdir/diag.sh init
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 1
$MainMsgQueueSaveOnShutdown on
$InputTCPServerRun 13514

$ModLoad ../plugins/omtesting/.libs/omtesting

# set spool locations and switch queue to disk-only mode
$WorkDirectory test-spool
$MainMsgQueueFilename mainq
$IncludeConfig work-queuemode.conf

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt

$IncludeConfig work-delay.conf
'
#export RSYSLOG_DEBUG="debug nologfuncflow nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"

# prepare config
echo \$MainMsgQueueType LinkedList > work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > work-delay.conf

# inject 10000 msgs, so that DO hit the high watermark
startup
. $srcdir/diag.sh injectmsg 0 10000
. $srcdir/diag.sh shutdown-immediate
wait_shutdown
. $srcdir/diag.sh check-mainq-spool
./mangle_qi -d -q test-spool/mainq.qi > tmp.qi
mv tmp.qi test-spool/mainq.qi

echo "Enter phase 2, rsyslogd restart"

# restart engine and have rest processed
#remove delay
echo "#" > work-delay.conf
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 9999 -d
exit_test
