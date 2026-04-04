#!/bin/bash
# Validate dirty-shutdown handling for a disk-assisted main queue.
#
# The historical version of this test assumed that a forced shutdown always
# left behind a fully recoverable queue that would later drain to an empty
# spool directory. Modern queue handling can also quarantine damaged queue
# state into mainq.bad.* when queue.onCorruption="safe" detects an
# inconsistent on-disk state. Both outcomes are valid as long as restart
# succeeds and no live mainq.* state is left behind.
# added 2016-12-01 by Rainer Gerhards
# rewritten 2026-03-30 to match current queue corruption semantics
# released under ASL 2.0

. ${srcdir:=.}/diag.sh init

SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
RSYSLOGD_LOG="${RSYSLOG_DYNNAME}.rsyslogd.log"
STARTED_LOG="${RSYSLOG_DYNNAME}.started"

write_phase1_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")

global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.filename="mainq"
	queue.saveOnShutdown="on"
	queue.timeoutShutdown="1"
	queue.maxFileSize="1m"
	queue.timeoutWorkerThreadShutdown="500"
	queue.size="200000"
)

:msg, contains, "msgnum:" :omtesting:sleep 10 0
'
}

write_phase2_conf() {
	generate_conf
	add_conf '
module(load="../plugins/omtesting/.libs/omtesting")

global(workDirectory="'"$SPOOL_DIR"'")
main_queue(
	queue.filename="mainq"
	queue.saveOnShutdown="on"
	queue.maxFileSize="1m"
	queue.timeoutWorkerThreadShutdown="500"
	queue.size="200000"
	queue.onCorruption="safe"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\\n")
action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

list_spool_top() {
	find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -printf '%f\n' | sort
}

count_bad_dirs() {
	find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 -type d -name 'mainq.bad.*' | wc -l
}

check_no_live_mainq_files() {
	local live_files

	live_files="$(find "$SPOOL_DIR" -maxdepth 1 -mindepth 1 \
		\( -type f -o -type l \) -name 'mainq*' -printf '%f\n')"
	if [ -n "$live_files" ]; then
		echo "FAIL: live mainq files remain after restart recovery"
		printf '%s\n' "$live_files"
		ls -l "$SPOOL_DIR"
		error_exit 1
	fi
}

write_phase1_conf
startup
injectmsg 0 210000

echo "spool files immediately before shutdown:"
list_spool_top

shutdown_immediate
./msleep 750

echo "spool files immediately after shutdown (before kill):"
list_spool_top

. "$srcdir/diag.sh" kill-immediate
wait_shutdown
rm -f "$RSYSLOG_PIDBASE.pid"

echo "spool files after kill:"
list_spool_top

if [ ! -f "$SPOOL_DIR/mainq.qi" ]; then
	echo "FAIL: .qi file does not exist after interrupted shutdown"
	error_exit 1
fi

echo ".qi file contents:"
cat "$SPOOL_DIR/mainq.qi"

printf 'RSYSLOG RESTART\n\n'
write_phase2_conf

: > "$RSYSLOGD_LOG"
: > "$STARTED_LOG"
export RS_REDIR=">>\"$RSYSLOGD_LOG\" 2>&1"

startup
shutdown_when_empty
wait_shutdown

unset RS_REDIR

echo "spool files after restart recovery:"
list_spool_top

check_no_live_mainq_files

bad_dirs="$(count_bad_dirs)"
if [ "$bad_dirs" -gt 0 ]; then
	echo "dirty shutdown caused queue quarantine into mainq.bad.*"
	if [ -f "$RSYSLOG_OUT_LOG" ]; then
		echo "recovered output was also written before quarantine cleanup"
	fi
	exit_test
fi

if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
	echo "FAIL: no recovered output data gathered and no quarantine directory created"
	ls -l "$SPOOL_DIR"
	error_exit 1
fi

exit_test
