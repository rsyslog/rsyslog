#!/bin/bash
# Test for queue data persisting at shutdown. LinkedList and FixedArray cases
# inspect the classic .qi format, so their modern queue configuration pins the
# classic DA engine. A fast shutdown must persist the remainder and restart
# must recover the complete sequence, with in-flight duplicates permitted.
# added 2009-05-27 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
# uncomment for debugging support:
echo testing memory queue persisting to disk, mode $1
disk_selector=
if [ "$1" != Disk ]; then
	# shellcheck disable=SC2089 # This is RainerScript text, expanded into add_conf below.
	disk_selector='queue.diskQueueType="disk"'
fi
generate_conf
# shellcheck disable=SC2090 # RainerScript quotes are intentionally retained.
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun '$TCPFLOOD_PORT'

$ModLoad ../plugins/omtesting/.libs/omtesting

# Configure either a classic DA memory queue or the pure classic disk queue.
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
main_queue(queue.type="'$1'" queue.filename="mainq"
	queue.timeoutShutdown="1" queue.saveOnShutdown="on"
	'$disk_selector')

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt

$IncludeConfig '${RSYSLOG_DYNNAME}'work-delay.conf
'
# prepare config
echo "*.*     :omtesting:sleep 0 1000" > ${RSYSLOG_DYNNAME}work-delay.conf

# inject 5000 msgs, so that we do not hit the high watermark
startup
injectmsg 0 5000
shutdown_immediate
wait_shutdown
check_mainq_spool

# restart engine and have rest processed
#remove delay
echo "#" > ${RSYSLOG_DYNNAME}work-delay.conf
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
./msleep 1000
wait_shutdown
# note: we need to permit duplicate messages, as due to the forced
# shutdown some messages may be flagged as "unprocessed" while they
# actually were processed. This is inline with rsyslog's philosophy
# to better duplicate than loose messages. Duplicate messages are
# permitted by the -d seq-check option.
seq_check 0 4999 -d
exit_test
