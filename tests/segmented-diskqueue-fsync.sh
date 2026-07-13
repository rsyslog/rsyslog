#!/bin/bash
# Exercise segmentedDisk with syncQueueFiles enabled. Exact sequence delivery
# plus forced-state-write statistics prove the synchronous durability path is
# active without relying on timing.
. ${srcdir:=.}/diag.sh init
require_plugin impstats
export NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats.log"
generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1")
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.syncQueueFiles="on" queue.timeoutShutdown="10000"
	queue.maxFileSize="32k" queue.checkpointInterval="64")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
wait_content 'state.forcedWrites=' "$STATS_FILE"
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
