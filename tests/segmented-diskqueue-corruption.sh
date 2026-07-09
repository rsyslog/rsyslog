#!/bin/bash
# Verify safe-mode recovery skips one payload-corrupt record and continues with
# later records in the same segment. The test inspector targets msgnum 200 and
# the oracle requires every other injected message, including the final one.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
check_command_available python3
export NUMMESSAGES=400
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.type="segmentedDisk"
	queue.filename="mainq"
	queue.maxFileSize="256k"
	queue.dequeueBatchSize="32"
	queue.saveOnShutdown="on"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$1"'
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
}

write_conf ':omtesting:sleep 10 0'
startup
injectmsg 0 "$NUMMESSAGES"
shutdown_immediate
. "$srcdir/diag.sh" kill-immediate
wait_shutdown

python3 "$srcdir/segdisk-inspect.py" "$SPOOL_DIR/mainq.segq" --corrupt-message-number 200 \
	> "${RSYSLOG_DYNNAME}.segdisk-inspect.json"

write_conf '# no delay during salvage'
startup
shutdown_when_empty
wait_shutdown

line_count=$(wc -l < "$RSYSLOG_OUT_LOG")
if [ "$line_count" -ne $((NUMMESSAGES - 1)) ]; then
	echo "FAIL: expected $((NUMMESSAGES - 1)) records after corruption salvage, got $line_count"
	error_exit 1
fi
if grep -qx 00000200 "$RSYSLOG_OUT_LOG"; then
	echo "FAIL: payload-corrupt message 200 was emitted"
	error_exit 1
fi
if ! grep -qx 00000399 "$RSYSLOG_OUT_LOG"; then
	echo "FAIL: salvage did not continue to the final record"
	error_exit 1
fi
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
