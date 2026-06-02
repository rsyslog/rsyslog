#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on all flavors of Solaris."

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 1
$MainMsgQueueSaveOnShutdown on
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$ModLoad ../plugins/omtesting/.libs/omtesting

# Configure a disk-assisted main queue.
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueType LinkedList
$MainMsgQueueFilename mainq

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt

$IncludeConfig '${RSYSLOG_DYNNAME}'work-delay.conf
'
#export RSYSLOG_DEBUG="debug nologfuncflow nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"

echo "*.*     :omtesting:sleep 0 1000" > ${RSYSLOG_DYNNAME}work-delay.conf

# Phase 1: inject enough messages to hit the high watermark and persist DA state.
startup
injectmsg 0 10000
shutdown_immediate
wait_shutdown
check_mainq_spool
./mangle_qi -d -q ${RSYSLOG_DYNNAME}.spool/mainq.qi > tmp.qi
mv tmp.qi ${RSYSLOG_DYNNAME}.spool/mainq.qi

echo "Enter phase 2, rsyslogd restart"

# Phase 2: remove the artificial delay and verify the mangled .qi recovers.
echo "#" > ${RSYSLOG_DYNNAME}work-delay.conf
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 9999 -d
exit_test
