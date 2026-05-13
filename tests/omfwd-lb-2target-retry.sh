#!/bin/bash
# added 2024-02-24 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=10000  # MUST be an EVEN number!

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
TARGET2_LISTEN_RELEASE="$RSYSLOG_DYNNAME.minitcpsrvr2.listen"
rm -f "$TARGET2_LISTEN_RELEASE"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2 "$TARGET2_LISTEN_RELEASE"

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
echo "waiting a bit to ensure rsyslog retries the currently suspended pool member"

sleep 3

injectmsg $NUMMESSAGES $NUMMESSAGES

shutdown_when_empty
wait_shutdown

if [ "$(wc -l < $RSYSLOG2_OUT_LOG)" != "$(( NUMMESSAGES / 2 ))" ]; then
	echo "ERROR: RSYSLOG2_OUT_LOG has invalid number of messages $(( NUMMESSAGES / 2 ))"
	cat -n  $RSYSLOG2_OUT_LOG | head -10
	error_exit 100
fi

# combine both files to check for correct message content
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
export NUMMESSAGES=$((NUMMESSAGES*2))
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check

exit_test
