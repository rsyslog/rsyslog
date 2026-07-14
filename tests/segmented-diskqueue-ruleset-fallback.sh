#!/bin/bash
# Persist events that entered through a named ruleset, then restart without
# that ruleset. Complete sequence recovery proves that a configuration rename
# falls back to the current default ruleset instead of classifying otherwise
# valid serialized events as corrupt. Duplicates are allowed because the first
# daemon is killed with an in-flight batch before its commit frontier advances.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="4k"
	queue.workerThreads="1"
	queue.dequeueBatchSize="1"
	queue.saveOnShutdown="on"
)
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
'
}

write_conf '
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port"
	ruleset="retired")
ruleset(name="retired") {
	:msg, contains, "msgnum:" :omtesting:sleep 10 0
}
'
startup
tcpflood -p"$TCPFLOOD_PORT" -m"$NUMMESSAGES"
# The sleeping single-record batch keeps one event in flight. Seeing the other
# 99 in the queue proves that imtcp handed the complete input to the durable
# store before the deliberate SIGKILL; tcpflood completion alone only proves
# delivery to the kernel socket buffer.
for _ in {1..100}; do
	queued=$(get_mainqueuesize | awk '{print $NF}')
	if ((queued >= NUMMESSAGES - 1)); then
		break
	fi
	"$TESTTOOL_DIR/msleep" 100
done
if ((queued < NUMMESSAGES - 1)); then
	echo "FAIL: only $queued of $((NUMMESSAGES - 1)) queued messages became durable"
	error_exit 1
fi
shutdown_immediate
. "$srcdir/diag.sh" kill-immediate
wait_shutdown

write_conf '
if ($msg contains "msgnum:") then
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check_dupes
wait_seq_check_dupes() {
	wait_seq_check 0 $((NUMMESSAGES - 1)) -d
}
startup
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
rm -rf "$SPOOL_DIR"
exit_test
