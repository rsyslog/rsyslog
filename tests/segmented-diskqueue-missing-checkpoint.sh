#!/bin/bash
# Verify corrupting both fixed-size state slots fails the queue immediately.
# A backlog is created first; the oracle is prompt startup failure with the
# offline-recovery diagnostic, never a synchronous segment scan.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="32k"
	queue.dequeueBatchSize="32"
	queue.saveOnShutdown="on"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 0 10000'
startup
wait_rsyslog_instance_pid
injectmsg 0 "$NUMMESSAGES"
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
if [ "$(wc -c < "$SPOOL_DIR/mainq.segq/state")" -ne 512 ]; then
	echo "FAIL: phase one did not create the two-slot state file"
	error_exit 1
fi
dd if=/dev/zero of="$SPOOL_DIR/mainq.segq/state" bs=512 count=1 conv=notrunc status=none

write_conf '# startup must fail before processing the backlog'
fail_log="${RSYSLOG_DYNNAME}.invalid-state.log"
../tools/rsyslogd -C -n -i"${RSYSLOG_DYNNAME}.invalid.pid" \
	-M../runtime/.libs:../.libs -f"${TESTCONF_NM}.conf" >"$fail_log" 2>&1 &
invalid_pid=$!
state_error_seen=0
for _ in {1..50}; do
	if grep -q "no valid state slot; offline recovery is required" "$fail_log"; then
		state_error_seen=1
		break
	fi
	sleep .1
done
kill -KILL "$invalid_pid" 2>/dev/null || true
wait "$invalid_pid" 2>/dev/null || true
rm -f "${RSYSLOG_DYNNAME}.invalid.pid"
if [ "$state_error_seen" -ne 1 ]; then
	echo "FAIL: invalid-state offline recovery diagnostic was not reported"
	cat "$fail_log"
	error_exit 1
fi
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
