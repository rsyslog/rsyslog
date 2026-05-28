#!/bin/bash
# Verifies that native omfwd load balancing can resume a target that was
# unavailable during the first message batch. Phase 1 keeps target 2 bound but
# not listening, so all initial messages must go to target 1. Phase 2 releases
# target 2, waits until minitcpsrv has actually called listen(), then waits
# until the one-second pool retry interval is certainly eligible in omfwd's
# whole-second timer model before injecting another full batch. After injection
# the test waits for minitcpsrv to accept the retry connection. Before shutdown,
# the test waits until both receivers show the expected exact line counts; this
# is the oracle that target 2 resumed and the second batch was split evenly
# rather than still flowing only to target 1.
# Added 2024-02-24 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=10000  # MUST be an EVEN number!

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
TARGET2_LISTEN_RELEASE="$RSYSLOG_DYNNAME.minitcpsrvr2.listen"
TARGET2_LISTEN_READY="$RSYSLOG_DYNNAME.minitcpsrvr2.ready"
TARGET2_ACCEPT_READY="$RSYSLOG_DYNNAME.minitcpsrvr2.accepted"
rm -f "$TARGET2_LISTEN_RELEASE"
rm -f "$TARGET2_LISTEN_READY"
rm -f "$TARGET2_ACCEPT_READY"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2 "$TARGET2_LISTEN_RELEASE" "$TARGET2_LISTEN_READY" "$TARGET2_ACCEPT_READY"

# regular startup
add_conf '
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueWorkerThreads 2

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
		protocol="tcp"
		pool.resumeInterval="1"
		action.resumeRetryCount="-1" action.resumeInterval="5")
}
'

startup
# we need special logic. In a first iteration, the second target is offline
# so everything is expected to go the the first target, only.
injectmsg
# combine both files to check for correct message content
#cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
wait_queueempty
wait_file_lines
cp "$RSYSLOG_OUT_LOG" tmp.log
seq_check
printf "\nSUCCESS for part 1 of the test\n\n"

# target2 has held its port bound but not listening since before rsyslog
# startup. This keeps phase 1 offline without exposing a get_free_port race.
touch "$TARGET2_LISTEN_RELEASE"
echo "waiting until the previously suspended pool member is listening"
wait_file_exists "$TARGET2_LISTEN_READY"
retry_ready_after=$(( $(date +%s) + 2 ))
echo "waiting until the target-pool retry interval is eligible"
while [ "$(date +%s)" -lt "$retry_ready_after" ]; do
	$TESTTOOL_DIR/msleep 100
done

injectmsg $NUMMESSAGES $NUMMESSAGES
echo "waiting until omfwd retries and reconnects the previously suspended pool member"
wait_file_exists "$TARGET2_ACCEPT_READY"

target2_expected=$(( NUMMESSAGES / 2 ))
target1_expected=$(( NUMMESSAGES + target2_expected ))
wait_file_lines --abort-on-oversize "$RSYSLOG2_OUT_LOG" "$target2_expected"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$target1_expected"

shutdown_when_empty
wait_shutdown

target2_actual="$(wc -l < "$RSYSLOG2_OUT_LOG")"
if [ "$target2_actual" != "$target2_expected" ]; then
	echo "ERROR: RSYSLOG2_OUT_LOG has invalid number of messages $target2_actual, expected $target2_expected"
	cat -n  $RSYSLOG2_OUT_LOG | head -10
	error_exit 100
fi

# combine both files to check for correct message content
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
export NUMMESSAGES=$((NUMMESSAGES*2))
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check

exit_test
