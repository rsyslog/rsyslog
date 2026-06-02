#!/bin/bash
# Test that imfile in inotify mode releases FDs of deleted files
# after FILE_DELETE_DELAY without requiring any additional inotify
# events. When no new events arrive after a file is deleted, poll()
# blocks indefinitely and the FD is never closed, leaking disk space.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. "${srcdir:=.}"/diag.sh init
. "$srcdir"/diag.sh check-inotify-only

export TESTMESSAGES=100
export RETRIES=50

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="inotify")
input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

./inputfilegen -m $TESTMESSAGES > "$RSYSLOG_DYNNAME".input
startup
wait_file_lines "$RSYSLOG_OUT_LOG" $TESTMESSAGES $RETRIES

PID=$(cat "$RSYSLOG_PIDBASE".pid)
if [ -z "$PID" ]; then
	printf 'FAIL: could not read rsyslog PID\n'
	error_exit 1
fi
check_fd_for_pid "$PID" exists ".input"

rm "$RSYSLOG_DYNNAME".input

# Wait for rsyslog to release the FD on its own — no new files, no
# trigger events.  FILE_DELETE_DELAY is 5 s, the fix caps poll() at
# FILE_DELETE_DELAY+1 s, so 20 s is more than enough.
max_wait=20
i=0
while ! check_fd_for_pid "$PID" absent ".input (deleted)"; do
	if [ "$i" -ge "$max_wait" ]; then
		printf 'FAIL: rsyslog did not release FD for deleted file after %d s\n' "$max_wait"
		find /proc/"$PID"/fd/ -lname '*deleted*' -ls 2>/dev/null
		error_exit 1
	fi
	./msleep 1000
	((i++))
done
printf 'PASS: FD released after %d s\n' "$i"

shutdown_when_empty
wait_shutdown
seq_check 0 $(( TESTMESSAGES - 1 ))
exit_test
