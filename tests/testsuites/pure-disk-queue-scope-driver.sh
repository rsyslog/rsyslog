#!/bin/bash
# Backend-neutral pure-disk queue scope driver. Thin wrappers select the Disk
# or segmentedDisk backend and main, ruleset, or action placement. The exact
# sequence and empty spool directory are the shared deterministic oracles.
. ${srcdir:=.}/diag.sh init
: "${QUEUE_TYPE:?QUEUE_TYPE is required}"
: "${QUEUE_SCOPE:?QUEUE_SCOPE is required}"
export NUMMESSAGES=1000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"

generate_conf
case "$QUEUE_SCOPE" in
main)
	add_conf '
global(workDirectory="'$SPOOL_DIR'")
main_queue(queue.type="'$QUEUE_TYPE'" queue.filename="scopeq" queue.maxFileSize="8k")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
	;;
ruleset)
	add_conf '
global(workDirectory="'$SPOOL_DIR'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="disk_scope" queue.type="'$QUEUE_TYPE'" queue.filename="scopeq" queue.maxFileSize="8k") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if ($msg contains "msgnum:") then call disk_scope
'
	;;
action)
	add_conf '
global(workDirectory="'$SPOOL_DIR'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		queue.type="'$QUEUE_TYPE'" queue.filename="scopeq" queue.maxFileSize="8k")
'
	;;
*)
	echo "FAIL: unknown queue scope $QUEUE_SCOPE"
	error_exit 1
	;;
esac

startup
injectmsg
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
if find "$SPOOL_DIR" -mindepth 1 -print -quit | grep -q .; then
	echo "FAIL: $QUEUE_TYPE $QUEUE_SCOPE queue left spool artifacts after clean drain"
	find "$SPOOL_DIR" -mindepth 1 -print
	error_exit 1
fi
rm -rf "$SPOOL_DIR"
exit_test
