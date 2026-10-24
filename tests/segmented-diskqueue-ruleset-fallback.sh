#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Persist events that entered through a named ruleset, then restart without
# that ruleset. Complete sequence recovery proves that a configuration rename
# falls back to the current default ruleset instead of classifying otherwise
# valid serialized events as corrupt. Duplicates are allowed because the first
# daemon is killed with an in-flight batch before its commit frontier advances.
# A FIFO action marker proves message 0 entered its callback; the remaining
# 99 queued entries then prove complete acceptance before the deliberate kill.
# Recovery after restart, rather than the queue count, proves persistence.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
ENTER_FILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASE_FIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASE_FIFO"
# Keep a local reader/writer open so failure cleanup never waits for a peer.
exec {release_fd}<>"$RELEASE_FIFO"
test_error_exit_handler() {
    printf 'release\n' >&"$release_fd" || true
}


write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
global(processInternalMessages="off")
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
	:msg, contains, "msgnum:00000000:" :omtesting:file_barrier '"$ENTER_FILE"' '"$RELEASE_FIFO"'
}
'
startup
tcpflood -p"$TCPFLOOD_PORT" -m1 -i0
wait_file_lines "$ENTER_FILE" 1
# The marker proves the first item is held in the single-record callback.
# Internal messages are disabled, so 99 queued entries can only be the
# remaining test records. Polling is bounded; it is not a timing oracle.
tcpflood -p"$TCPFLOOD_PORT" -m"$((NUMMESSAGES - 1))" -i1
for _ in {1..100}; do
	queued=$(get_mainqueuesize | awk '{print $NF}')
	if ((queued >= NUMMESSAGES - 1)); then
		break
	fi
	"$TESTTOOL_DIR/msleep" 100
done
if ((queued < NUMMESSAGES - 1)); then
	echo "FAIL: only $queued of $((NUMMESSAGES - 1)) remaining messages were accepted"
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
exec {release_fd}>&-
rm -f "$ENTER_FILE" "$RELEASE_FIFO"
exit_test
