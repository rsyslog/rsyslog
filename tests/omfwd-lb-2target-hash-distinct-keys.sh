#!/bin/bash
# Verifies that pool.policy="hash" actually reads pool.hashkey's rendered
# value, not just that SOME target gets pinned. omfwd-lb-2target-hash.sh only
# asserts "all-or-nothing", a property an implementation that ignores the key
# entirely (e.g. always hashing NULL) would satisfy just as well. This test
# requires two DIFFERENT keys to land on two DIFFERENT targets, which a stub
# that ignores the key cannot fake. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=500

# minitcpsrv terminates itself once its one connection closes (rsyslogd exits
# between phase 1 and phase 2, closing it); -K keeps it listening across that
# gap so both phases can reuse the same two ports.
KEEP_RUNNING_FILE="$RSYSLOG_DYNNAME.minitcpsrvr.keeprunning"
touch "$KEEP_RUNNING_FILE"
MINITCPSRV_EXTRA_OPTS="-K $KEEP_RUNNING_FILE"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2

echo 'template(name="shardkey" type="string" string="target-a-key")' > ${RSYSLOG_DYNNAME}.shardkey.conf

add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")
$IncludeConfig '${RSYSLOG_DYNNAME}'.shardkey.conf

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
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

n1a=$(wc -l < $RSYSLOG_OUT_LOG)
n2a=$(wc -l < $RSYSLOG2_OUT_LOG)
printf 'key A (target-a-key): target1=%s target2=%s\n' "$n1a" "$n2a"

echo "Enter phase 2, rsyslogd restart with a different pool.hashkey value"
echo 'template(name="shardkey" type="string" string="target-b-key")' > ${RSYSLOG_DYNNAME}.shardkey.conf

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

n1=$(wc -l < $RSYSLOG_OUT_LOG)
n2=$(wc -l < $RSYSLOG2_OUT_LOG)
n1b=$((n1 - n1a))
n2b=$((n2 - n2a))
printf 'key B (target-b-key): target1=%s target2=%s (delta over phase 1)\n' "$n1b" "$n2b"

# each key must still pin all-or-nothing, same property as omfwd-lb-2target-hash.sh
if ! { [ "$n1a" -eq "$NUMMESSAGES" ] && [ "$n2a" -eq 0 ]; } &&
	! { [ "$n2a" -eq "$NUMMESSAGES" ] && [ "$n1a" -eq 0 ]; }; then
	echo "ERROR: key A did not pin all messages to a single target"
	error_exit 100
fi
if ! { [ "$n1b" -eq "$NUMMESSAGES" ] && [ "$n2b" -eq 0 ]; } &&
	! { [ "$n2b" -eq "$NUMMESSAGES" ] && [ "$n1b" -eq 0 ]; }; then
	echo "ERROR: key B did not pin all messages to a single target"
	error_exit 100
fi

# ...and the two keys must NOT pin to the SAME target -- this is the part a
# key-blind stub cannot fake.
if { [ "$n1a" -eq "$NUMMESSAGES" ] && [ "$n1b" -eq "$NUMMESSAGES" ]; } ||
	{ [ "$n2a" -eq "$NUMMESSAGES" ] && [ "$n2b" -eq "$NUMMESSAGES" ]; }; then
	echo "ERROR: two different routing keys pinned to the SAME target -- pool.hashkey may not be read"
	error_exit 100
fi

rm -f "$KEEP_RUNNING_FILE"
stop_minitcpsrvrs
exit_test
