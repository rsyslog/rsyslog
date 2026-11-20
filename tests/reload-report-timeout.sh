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
#  - afterwards no "-N1" child process of this test exists (it was killed
#    and reaped, not left as a zombie: a zombie would still show in /proc);
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
rr_wait_count 'validation of the changed configuration timed out after 1 seconds, running configuration kept' "$RR_STARTED" 1
if [ -n "$(rr_child_pids)" ]; then
	echo "FAIL: timed-out validation child still exists: $(rr_child_pids)"
	error_exit 1
fi
injectmsg 50 50
shutdown_when_empty
wait_shutdown
seq_check 0 99
exit_test
