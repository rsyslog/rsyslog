#!/bin/bash
# Test the pipe output action with a disk-backed main queue.
#
# The test creates a FIFO, lets rsyslog write formatted messages to it, and
# copies the FIFO stream into the normal sequence-check output file. Success
# requires all injected messages to reach the external FIFO reader before
# shutdown; main-queue-empty alone is not enough because the pipe reader is
# outside rsyslog's queue accounting.
# added 2009-11-05 by RGerhards

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20000
generate_conf
if [ "${QUEUE_TYPE:-disk}" = "segmentedDisk" ]; then
	add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then action(type="ompipe" pipe="rsyslog-testbench-fifo" template="outfmt")
'
else
	add_conf '
$MainMsgQueueTimeoutShutdown 10000

# set spool locations and switch queue to disk-only mode
$WorkDirectory '$RSYSLOG_DYNNAME'.spool
$MainMsgQueueFilename mainq
$MainMsgQueueType disk

$template outfmt,"%msg:F,58:2%\n"
# with pipes, we do not need to use absolute path names, so
# we can simply refer to our working pipe via the usual relative
# path name
:msg, contains, "msgnum:" |rsyslog-testbench-fifo;outfmt
'
fi
rm -f rsyslog-testbench-fifo
mkfifo rsyslog-testbench-fifo
cp rsyslog-testbench-fifo  $RSYSLOG_OUT_LOG &
CPPROCESS=$!
echo background cp process id is $CPPROCESS

# now do the usual run
startup
injectmsg 0 $NUMMESSAGES
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo waiting for background cp to terminate...
wait $CPPROCESS
rm -f rsyslog-testbench-fifo
echo background cp has terminated, continue test...

# and continue the usual checks
seq_check
exit_test
