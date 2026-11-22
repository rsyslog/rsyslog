#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: with config.reloadOnHUP unset (default) or "off", a HUP behaves as
# before the feature existed: no validation child, no new log line, even if
# a config file changed. The running config keeps processing messages.
#
# Setup: the setting is omitted (this script) or given as "off" (the YAML
# twin). The include fragment is changed validly, then invalidly, with a HUP
# after each change.
#
# Oracle:
#  - after each HUP completed, no "config reload check:" line exists and no
#    "-N1" child process of this test exists. A wrongly started child would
#    need some 100 ms to finish and report, so the negative check waits a
#    fixed 1 s window before asserting (a negative assertion cannot be awaited
#    event-driven; the window only makes a false pass unlikely, a failure is
#    always real);
#  - all injected messages arrive.
. ${srcdir:=.}/diag.sh init
. ${srcdir}/reload-report-common.sh
[ -r /proc/self/cmdline ] || skip_test # the no-child oracle needs /proc
rr_setup "${RR_MODE:-}"
startup
injectmsg 0 50
rr_inc_valid 2
issue_HUP
rr_inc_invalid
issue_HUP
$TESTTOOL_DIR/msleep 1000
rr_assert_count "$RR_REPORT" "$RR_STARTED" 0
if [ -n "$(rr_child_pids)" ]; then
	echo "FAIL: a validation child exists although config.reloadOnHUP is not \"report\""
	error_exit 1
fi
injectmsg 50 50
shutdown_when_empty
wait_shutdown
seq_check 0 99
exit_test
