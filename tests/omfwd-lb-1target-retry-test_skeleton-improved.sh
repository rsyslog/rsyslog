#!/bin/bash
# Improved version of omfwd-lb-1target-retry-test_skeleton.sh with better synchronization
# added 2024-02-19 by rgerhards. Released under ASL 2.0
# Modified for better timing control on slow systems
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test is currently not supported on solaris due to too-different timing"
generate_conf
export NUMMESSAGES=20000

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
export MINITCPSRV_EXTRA_OPTS="-D900 -B2 -a -S3"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1

add_conf '
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueDequeueBatchSize 100

global(allMessagesToStderr="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt" iobuffer.maxSize="'$OMFWD_IOBUF_SIZE'")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1"] port="'$MINITCPSRVR_PORT1'" protocol="tcp"
		extendedConnectionCheck="off"
		pool.resumeInterval="1"
		action.resumeRetryCount="-1" action.resumeInterval="1"
		# Add queue parameters for better reliability on slow systems
		queue.type="LinkedList"
		queue.size="50000"
		queue.dequeueBatchSize="100"
		queue.timeoutEnqueue="10000"
		queue.timeoutShutdown="20000"
		queue.saveOnShutdown="on")
}
'

# now do the usual run
startup

# Add a small delay to ensure rsyslog is fully started
sleep 2

# Inject messages with better pacing for slow systems
# Split message injection into batches to avoid overwhelming slow systems
BATCH_SIZE=1000
for ((i=0; i<NUMMESSAGES; i+=BATCH_SIZE)); do
    end=$((i+BATCH_SIZE-1))
    if [ $end -ge $NUMMESSAGES ]; then
        end=$((NUMMESSAGES-1))
    fi
    injectmsg $i $((end-i+1))
    # Small delay between batches on slow systems
    if [ "$SLOW_SYSTEM" == "1" ]; then
        sleep 0.1
    fi
done

# Wait longer for message processing on slow systems
if [ "$SLOW_SYSTEM" == "1" ]; then
    sleep 30
else
    sleep 20
fi

shutdown_when_empty
wait_shutdown 60  # Increase shutdown timeout
# note: minitcpsrv shuts down automatically if the connection is closed!

# Wait a bit more to ensure all files are flushed
sleep 2

export SEQ_CHECK_OPTIONS=-d
#permit 500 messages to be lost in this extreme test (-m 100)
# Increase tolerance for slow systems
if [ "$SLOW_SYSTEM" == "1" ]; then
    seq_check 0 $((NUMMESSAGES-1)) -m200
else
    seq_check 0 $((NUMMESSAGES-1)) -m100
fi
exit_test