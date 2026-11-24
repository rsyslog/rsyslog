#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: global(config.reloadOnHUP="report") validates a changed config on
# HUP in a child "rsyslogd -N1" and reports the outcome in one log line. The
# running config is never replaced and message processing continues.
#
# Setup: the config includes a fragment file and a glob directory that is
# empty at startup. Relative include paths are used on purpose (rsyslogd
# chdir()s to "/" after loading).
#
# Oracle (the child runs asynchronously, so each expected line is awaited with
# a bounded wait, see rr_wait_count; the HUP itself is awaited exactly by
# issue_HUP via imdiag):
#  1. HUP with unchanged files: no report line. Unchanged HUPs are decided
#     synchronously inside the HUP, so the check right after issue_HUP is exact.
#  2. Fragment edited validly: one "configuration changed and is valid" line.
#  3. Fragment broken: one "changed configuration is invalid" line that names
#     an error; messages injected afterwards still arrive (running config kept).
#  4. Fragment fixed again and a new file added to the include glob: valid
#     line #2 (a new glob match counts as change).
#  5. Everything restored to the original content (mtime differs, content
#     equal): "unchanged", no new line.
# Finally all injected messages must be in the output (seq_check).
. ${srcdir:=.}/diag.sh init
. ${srcdir}/reload-report-common.sh
rr_setup report
startup
injectmsg 0 50

issue_HUP
rr_assert_count "$RR_REPORT" "$RR_STARTED" 0

rr_inc_valid 2
issue_HUP
rr_wait_count 'configuration changed and is valid; restart rsyslog to apply it' "$RR_STARTED" 1

rr_inc_invalid
issue_HUP
rr_wait_count 'changed configuration is invalid, running configuration kept' "$RR_STARTED" 1
if ! grep -i 'invalid, running configuration kept: .*error' "$RR_STARTED"; then
	echo "FAIL: invalid report does not contain the child's error text"
	cat -n "$RR_STARTED"
	error_exit 1
fi
injectmsg 50 50

rr_inc_valid 3
rr_glob_add
issue_HUP
rr_wait_count 'configuration changed and is valid' "$RR_STARTED" 2

rr_inc_valid 1
rm -f "$RR_DIR"/add.*
issue_HUP
rr_assert_count "$RR_REPORT" "$RR_STARTED" 3

shutdown_when_empty
wait_shutdown
seq_check 0 99
exit_test
