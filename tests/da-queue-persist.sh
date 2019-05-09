#!/bin/bash
# Test for DA queue data persisting at shutdown. The
# plan is to start an instance, emit some data, do a relatively
# fast shutdown and then re-start the engine to process the 
# remaining data.
# added 2019-05-08 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="linkedList" queue.filename="mainq"
	queue.timeoutShutdown="1" queue.saveonshutdown="on")

module(load="../plugins/omtesting/.libs/omtesting")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsg")

$IncludeConfig '${RSYSLOG_DYNNAME}'work-delay.conf
'

# prepare config
echo "*.*     :omtesting:sleep 0 1000" > ${RSYSLOG_DYNNAME}work-delay.conf

startup
injectmsg 0 $NUMMESSAGES
shutdown_immediate
wait_shutdown
check_mainq_spool

echo "Enter phase 2, rsyslogd restart"
# note: we may have some few duplicates!
echo "#" > ${RSYSLOG_DYNNAME}work-delay.conf
wait_seq_check_with_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_with_dupes
startup
shutdown_when_empty
seq_check 0 $((NUMMESSAGES - 1)) -d
content_check "queue files exist on disk, re-starting with" $RSYSLOG_DYNNAME.othermsg
exit_test
