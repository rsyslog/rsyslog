#!/bin/bash
# Verify that native omfwd load balancing splits a single message burst evenly
# across two healthy TCP targets. The oracle is exact: each minitcpsrv receiver
# must persist half of the messages, and the combined output must contain the
# full sequence. Wait for the receiver files before shutdown so the test does
# not mistake an action/output drain still in progress for a load-balancing
# failure.
# Added 2024-02-24 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=1000  # MUST be an EVEN number!

# starting minitcpsrvr receives so that we can obtain their port
# numbers
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2

# regular startup
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
		protocol="tcp"
		pool.resumeInterval="10"
		action.resumeRetryCount="-1" action.resumeInterval="5")
}
'

# now do the usual run
startup
injectmsg

expected_per_target=$(( NUMMESSAGES / 2 ))
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$expected_per_target"
wait_file_lines --abort-on-oversize "$RSYSLOG2_OUT_LOG" "$expected_per_target"

shutdown_when_empty
wait_shutdown

target1_actual="$(wc -l < "$RSYSLOG_OUT_LOG")"
if [ "$target1_actual" -ne "$expected_per_target" ]; then
	echo "ERROR: RSYSLOG_OUT_LOG has invalid number of messages $target1_actual, expected $expected_per_target"
	error_exit 100
fi

target2_actual="$(wc -l < "$RSYSLOG2_OUT_LOG")"
if [ "$target2_actual" -ne "$expected_per_target" ]; then
	echo "ERROR: RSYSLOG2_OUT_LOG has invalid number of messages $target2_actual, expected $expected_per_target"
	error_exit 100
fi

# combine both files to check for correct message content
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"

seq_check
exit_test
