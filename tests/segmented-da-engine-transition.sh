#!/bin/bash
# Build a real classic disk-assisted action backlog, then restart with the auto
# selector. The marker, warning, and complete drain prove sticky classic
# compatibility. A final clean restart with auto-upgrade enabled must replace
# only the drained marker and select an unmaterialized segmented child. The
# seed uses checkpointInterval=1; an interrupted in-flight batch may replay,
# so duplicates are accepted but every sequence number must be present.
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=3000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
MARKER="$SPOOL_DIR/transition.da-engine"

write_conf() {
	local selector="$1" upgrade="$2"
	local upgrade_config=
	[ "$selector" != auto ] || upgrade_config="queue.diskQueueAutoUpgrade=\"$upgrade\""
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="LinkedList" queue.filename="transition"
	queue.diskQueueType="'"$selector"'" '"$upgrade_config"'
	queue.size="50" queue.highWatermark="10" queue.lowWatermark="5"
	queue.checkpointInterval="1" queue.saveOnShutdown="on"
	queue.timeoutShutdown="10000")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

write_seed_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="Disk" queue.filename="transition"
	queue.checkpointInterval="1" queue.saveOnShutdown="on"
	queue.timeoutShutdown="1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:omtesting:sleep 0 5000
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

# Seed the spool through the classic pure-disk path. Every accepted message is
# durable before injectmsg returns, avoiding a shutdown race in the DA transfer
# layer while producing the exact legacy artifacts that auto-detection sees.
write_seed_conf
startup
injectmsg
shutdown_immediate
wait_shutdown
[ -f "$SPOOL_DIR/transition.qi" ] || error_exit 1 "classic backlog was not persisted"

write_conf auto off
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
seq_check 0 $((NUMMESSAGES - 1)) -d
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 disk" ] || error_exit 1 "classic marker was not sticky"
[ ! -f "$SPOOL_DIR/transition.qi" ] || error_exit 1 "classic state remained after a clean drain"

write_conf auto on
startup
[ "$(cat "$MARKER")" = "RSYSLOG-DA-ENGINE-V1 segmentedDisk" ] ||
	error_exit 1 "auto-upgrade did not select segmentedDisk after classic drain"
[ ! -e "$SPOOL_DIR/transition.segq" ] || error_exit 1 "auto-upgraded child materialized without a spill"
shutdown_when_empty
wait_shutdown
exit_test
