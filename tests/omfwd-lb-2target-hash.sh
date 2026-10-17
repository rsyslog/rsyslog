#!/bin/bash
# Verifies omfwd pool.policy="hash": a constant routing key must pin ALL
# messages to a SINGLE target of the pool (round-robin would split them 50/50,
# see omfwd-lb-2target-basic.sh). Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=1000

# start the two pool targets so we can obtain their port numbers
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2

add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
template(name="shardkey" type="string" string="constant-shard-key")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
		protocol="tcp"
		pool.policy="hash" pool.hashkey="shardkey"
		pool.resumeInterval="10"
		action.resumeRetryCount="-1" action.resumeInterval="5")
}
'

startup
injectmsg
shutdown_when_empty
wait_shutdown

n1=$(wc -l < $RSYSLOG_OUT_LOG)
n2=$(wc -l < $RSYSLOG2_OUT_LOG)
printf 'pool.policy=hash constant-key split: target1=%s target2=%s (expect all-or-nothing)\n' "$n1" "$n2"

# a constant routing key must resolve to exactly ONE target: all messages there,
# none on the other. (Round-robin would put NUMMESSAGES/2 on each.)
if ! { [ "$n1" -eq "$NUMMESSAGES" ] && [ "$n2" -eq 0 ]; } &&
	! { [ "$n2" -eq "$NUMMESSAGES" ] && [ "$n1" -eq 0 ]; }; then
	echo "ERROR: pool.policy=hash with a constant key did not pin all messages to a single target"
	error_exit 100
fi

# and nothing is lost or duplicated: the receiving target holds the full sequence
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"
seq_check
exit_test
