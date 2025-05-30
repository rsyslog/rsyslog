#!/bin/bash
# added 2024-02-24 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=10000  # MUST be an EVEN number!

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
start_minitcpsrvr $RSYSLOG_OUT_LOG  1

# regular startup
add_conf '
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueWorkerThreads 2

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$TCPFLOOD_PORT'"]
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

echo WARNING: The next part of this test is flacky, because there is an
echo inevitable race on the port number for minitcpsrvr. If another
echo parallel test has aquired it in the interim, this test here will
echo invalidly fail.
./minitcpsrv -t127.0.0.1 -p $TCPFLOOD_PORT -f "$RSYSLOG2_OUT_LOG" \
	-P "$RSYSLOG_DYNNAME.minitcpsrvr_port2"  &
# Note: we use the port file just to make sure minitcpsrvr has initialized!
wait_file_exists "$RSYSLOG_DYNNAME.minitcpsrvr_port2"
BGPROCESS=$!
echo "### background minitcpsrv process id is $BGPROCESS port $TCPFLOOD_PORT ###"
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
