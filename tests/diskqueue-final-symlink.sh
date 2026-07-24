#!/bin/bash
# Verify that a persistent disk queue rejects a symlinked segment file. The
# first phase saves a real segmented queue; the second replaces its newest
# final-path-component segment with a link to a copy. A legacy queue would
# resume through the link and append the new message to that copied segment.
# The oracle is the target checksum after shutdown, so a daemon diagnostic or
# timing-dependent failure cannot make the test pass. The 20ms action delay
# makes the 100-message setup batch remain on disk long enough to preserve
# multiple 16KiB segments before the deliberately immediate shutdown.
# added 2026-07-24 by Codex, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
TARGET_FILE="${RSYSLOG_DYNNAME}.queue-target"
TARGET_SUM="${RSYSLOG_DYNNAME}.queue-target.sum"

write_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")

main_queue(
	queue.type="disk"
	queue.filename="mainq"
	queue.maxFileSize="16k"
	queue.saveOnShutdown="on"
	queue.timeoutShutdown="1"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\\n")
:omtesting:sleep 0 20000
if ($msg contains "msgnum:") then {
	action(type="omfile" template="outfmt" file="'"$RSYSLOG_OUT_LOG"'")
}
'
}

rm -rf "$SPOOL_DIR"
rm -f "$TARGET_FILE" "$TARGET_SUM"

write_conf
startup
injectmsg 0 100
shutdown_immediate
wait_shutdown

SEGMENT_FILE=$(find "$SPOOL_DIR" -maxdepth 1 -type f -name 'mainq.[0-9]*' | sort | tail -n 1)
if [ -z "$SEGMENT_FILE" ]; then
	echo "FAIL: first phase did not preserve a disk queue segment"
	ls -l "$SPOOL_DIR"
	error_exit 1
fi

cp "$SEGMENT_FILE" "$TARGET_FILE" || error_exit 1
cksum "$TARGET_FILE" > "$TARGET_SUM" || error_exit 1
rm -f "$SEGMENT_FILE"
ln -s "../$TARGET_FILE" "$SEGMENT_FILE" || error_exit 1

write_conf
startup
injectmsg 100 1
shutdown_immediate
wait_shutdown

if ! cksum "$TARGET_FILE" | cmp -s - "$TARGET_SUM"; then
	echo "FAIL: disk queue followed final-component segment symlink"
	error_exit 1
fi

exit_test
