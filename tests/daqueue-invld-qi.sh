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

# set spool locations and switch queue to disk-only mode
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueFilename mainq
$IncludeConfig '${RSYSLOG_DYNNAME}'work-queuemode.conf

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt

$IncludeConfig '${RSYSLOG_DYNNAME}'work-delay.conf
'
#export RSYSLOG_DEBUG="debug nologfuncflow nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"

# prepare config
echo \$MainMsgQueueType LinkedList > ${RSYSLOG_DYNNAME}work-queuemode.conf
echo "*.*     :omtesting:sleep 0 1000" > ${RSYSLOG_DYNNAME}work-delay.conf

# inject 10000 msgs, so that DO hit the high watermark
startup
injectmsg 0 10000
shutdown_immediate
wait_shutdown
check_mainq_spool
./mangle_qi -d -q ${RSYSLOG_DYNNAME}.spool/mainq.qi > tmp.qi
mv tmp.qi ${RSYSLOG_DYNNAME}.spool/mainq.qi

echo "Enter phase 2, rsyslogd restart"

# restart engine and have rest processed
#remove delay
echo "#" > ${RSYSLOG_DYNNAME}work-delay.conf
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 9999 -d
exit_test
