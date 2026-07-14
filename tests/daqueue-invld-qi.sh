#!/bin/bash
# Exercise classic DA recovery from a deliberately mangled .qi file. This test
# inspects classic state directly, so the modern queue configuration pins the
# classic engine; restart must recover the full sequence, allowing duplicates.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on all flavors of Solaris."

generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$ModLoad ../plugins/omtesting/.libs/omtesting

# Configure a classic disk-assisted main queue. Keep this fully in the modern
# frontend because diskQueueType intentionally has no legacy directive alias.
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.timeoutShutdown="1" queue.saveOnShutdown="on"
	queue.diskQueueType="disk")

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
