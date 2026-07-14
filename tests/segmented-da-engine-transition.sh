#!/bin/bash
# Build a real classic disk-assisted action backlog, then restart with the auto
# selector. The marker, warning, and exact drain prove sticky classic
# compatibility. A final clean restart with auto-upgrade enabled must replace
# only the drained marker and select an unmaterialized segmented child.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=3000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
MARKER="$SPOOL_DIR/transition.da-engine"

write_conf() {
	local selector="$1" upgrade="$2" slowdown="$3"
	local delay_rule=
	local upgrade_config=
	[ "$slowdown" = 0 ] || delay_rule=":omtesting:sleep 0 $slowdown"
	[ "$selector" != auto ] || upgrade_config="queue.diskQueueAutoUpgrade=\"$upgrade\""
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="LinkedList" queue.filename="transition"
	queue.diskQueueType="'"$selector"'" '"$upgrade_config"'
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.checkpointInterval="1" queue.saveOnShutdown="on"
	queue.timeoutShutdown="1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
'"$delay_rule"'
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

write_conf disk off 5000
startup
injectmsg
shutdown_immediate
wait_shutdown
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 disk" ] || error_exit 1 "classic marker missing"
[ -f "$SPOOL_DIR/transition.qi" ] || error_exit 1 "classic DA backlog was not persisted"

write_conf auto off 0
AUTO_LOG="${RSYSLOG_DYNNAME}.auto.log"
export RS_REDIR=">\"$AUTO_LOG\" 2>&1"
startup
shutdown_when_empty
wait_shutdown
unset RS_REDIR
grep -Fq 'automatically selected the classic disk engine' "${RSYSLOG_DYNNAME}.started" || {
	cat "${RSYSLOG_DYNNAME}.started"
	error_exit 1 "automatic classic selection warning missing"
}
seq_check 0 $((NUMMESSAGES - 1))
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 disk" ] || error_exit 1 "classic marker was not sticky"
[ ! -f "$SPOOL_DIR/transition.qi" ] || error_exit 1 "classic state remained after a clean drain"

write_conf auto on 0
startup
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 segmentedDisk" ] ||
	error_exit 1 "auto-upgrade did not select segmentedDisk after classic drain"
[ ! -e "$SPOOL_DIR/transition.segq" ] || error_exit 1 "auto-upgraded child materialized without a spill"
shutdown_when_empty
wait_shutdown
exit_test
