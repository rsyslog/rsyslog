#!/bin/bash
# Shared fail-fast startup driver for missing state and experimental v1 state.
# A real SIGKILL leaves segment data; startup must emit the selected offline-
# recovery diagnostic promptly rather than scanning payloads.
. ${srcdir:=.}/diag.sh init
: "${STATE_MUTATION:?STATE_MUTATION is required}"
export NUMMESSAGES=300
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxFileSize="8k" queue.saveOnShutdown="on")
:omtesting:sleep 10 0
'
}

write_conf
startup
wait_rsyslog_instance_pid
injectmsg
. "$srcdir/diag.sh" kill-immediate
wait_shutdown
rm -f "$SPOOL_DIR/mainq.segq/state"
case "$STATE_MUTATION" in
missing)
	expected='state is missing while queue files exist; offline recovery is required'
	;;
v1)
	: >"$SPOOL_DIR/mainq.segq/meta"
	expected='experimental segmentedDisk v1 state is incompatible with store format v2'
	;;
*)
	echo "FAIL: unknown state mutation $STATE_MUTATION"
	error_exit 1
	;;
esac

fail_log="${RSYSLOG_DYNNAME}.${STATE_MUTATION}-state.log"
../tools/rsyslogd -C -n -i"${RSYSLOG_DYNNAME}.invalid.pid" \
	-M../runtime/.libs:../.libs -f"${TESTCONF_NM}.conf" >"$fail_log" 2>&1 &
invalid_pid=$!
seen=0
for _ in {1..50}; do
	if grep -qF "$expected" "$fail_log"; then
		seen=1
		break
	fi
	sleep .1
done
kill -KILL "$invalid_pid" 2>/dev/null || true
wait "$invalid_pid" 2>/dev/null || true
rm -f "${RSYSLOG_DYNNAME}.invalid.pid"
if [ "$seen" -ne 1 ]; then
	echo "FAIL: expected fail-fast $STATE_MUTATION state diagnostic"
	cat "$fail_log"
	error_exit 1
fi
rm -rf "$SPOOL_DIR"
exit_test
