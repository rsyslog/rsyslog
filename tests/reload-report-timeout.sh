#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: a validation child that does not finish within its deadline is
# killed and reaped, reported as "timed out", and the daemon keeps running.
#
# The child is blocked on purpose: the changed include fragment defines a
# lookup table whose file is a FIFO that nobody opens (see
# reload-report-common.sh). The deadline is shortened to 1 s with the
# RSYSLOG_RELOAD_CHECK_TIMEOUT_MS diagnostic variable (default: 30 s).
#
# Oracle (bounded waits, the timeout is the event under test):
#  - a "timed out" report line appears;
#  - afterwards the /proc entry of the child (pid captured while it was
#    blocked) is gone: killed and reaped, not left as a zombie;
#  - messages injected before and after are all processed.
. ${srcdir:=.}/diag.sh init
. ${srcdir}/reload-report-common.sh
[ -r /proc/self/cmdline ] || skip_test
export RSYSLOG_RELOAD_CHECK_TIMEOUT_MS=1000
rr_setup report
startup
injectmsg 0 50
mkfifo "$RR_FIFO"
rr_inc_block
issue_HUP
rr_wait_child
child_pid=$(rr_child_pids | head -n 1)
rr_wait_count 'validation of the changed configuration timed out after 1 seconds, running configuration kept' "$RR_STARTED" 1
# A zombie has an empty cmdline, so check the /proc entry of the captured pid:
# it must be gone (killed and reaped).
if [ -e "/proc/$child_pid" ]; then
	echo "FAIL: timed-out validation child $child_pid still has a /proc entry (not reaped?)"
	error_exit 1
fi
injectmsg 50 50
shutdown_when_empty
wait_shutdown
seq_check 0 99
exit_test
