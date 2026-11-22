#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: a burst of HUPs while a validation child runs is coalesced, the
# outcome of identical consecutive checks is logged once, and a running child
# is killed and reaped on shutdown.
#
# The child is held at a point of the test's choosing: the changed include
# fragment defines a lookup table whose file is a FIFO, which blocks the
# child's config load in open() until the test opens the other end (see
# reload-report-common.sh). A FIFO has size 0, so once released the child
# reports "lookup table file ... is empty" and exits non-zero: a deterministic
# "invalid" verdict. Counters come from impstats ("config.reload.check").
#
# Oracle:
#  1. Fragment changed to use the FIFO, one HUP: child starts (attempted=1)
#     and blocks.
#  2. 20 more HUPs are sent back to back, then one awaited via issue_HUP: at
#     least one is counted as coalesced and attempted is still 1 (a blocked
#     child cannot finish, so the bound is exact, not timing based).
#  3. FIFO opened by the test: child ends, exactly one "invalid" report that
#     names the lookup file. No rerun happens (attempted stays 1) because the
#     files did not change since the child was started.
#  4. FIFO replaced by an empty regular file, HUP: second child, same invalid
#     outcome (invalid=2) but still only one "invalid" line in the log.
#  5. Empty file replaced by valid lookup JSON, HUP: third child, a different
#     outcome, so one "valid" line (attempted=3).
#  6. FIFO again, HUP: fourth child blocks (a /proc scan sees it); shutdown
#     must complete and leave no validation child behind.
. ${srcdir:=.}/diag.sh init
. ${srcdir}/reload-report-common.sh
[ -r /proc/self/cmdline ] || skip_test
rr_setup report stats
startup

mkfifo "$RR_FIFO"
rr_inc_block
issue_HUP
rr_wait_stat attempted 1
rr_wait_child
for i in $(seq 20); do
	kill -HUP "$(cat $RSYSLOG_PIDBASE.pid)"
done
issue_HUP
rr_wait_stat coalesced 1
rr_assert_count "$RR_REPORT" "$RR_STARTED" 0
if [ "$(rr_stat attempted)" != "1" ]; then
	echo "FAIL: attempted must stay 1 while the child is blocked, is $(rr_stat attempted)"
	error_exit 1
fi

printf '' > "$RR_FIFO"
rr_wait_count 'changed configuration is invalid, running configuration kept' "$RR_STARTED" 1
rr_wait_stat invalid 1
if ! grep -F "$RR_FIFO" "$RR_STARTED" | grep -q 'is empty'; then
	echo "FAIL: invalid report does not name the lookup file problem"
	cat -n "$RR_STARTED"
	error_exit 1
fi
if [ "$(rr_stat attempted)" != "1" ]; then
	echo "FAIL: no rerun expected, attempted is $(rr_stat attempted)"
	error_exit 1
fi

rm -f "$RR_FIFO"
: > "$RR_FIFO"
issue_HUP
rr_wait_stat invalid 2
rr_assert_count 'changed configuration is invalid' "$RR_STARTED" 1

rr_lookup_content > "$RR_FIFO"
issue_HUP
rr_wait_count 'configuration changed and is valid' "$RR_STARTED" 1
rr_wait_stat valid_changed 1 # the same statistics line also shows attempted
if [ "$(rr_stat attempted)" != "3" ]; then
	echo "FAIL: expected exactly 3 validation children so far, attempted is $(rr_stat attempted)"
	error_exit 1
fi

rm -f "$RR_FIFO"
mkfifo "$RR_FIFO"
issue_HUP
rr_wait_child
shutdown_when_empty
wait_shutdown
if [ -n "$(rr_child_pids)" ]; then
	echo "FAIL: validation child survived shutdown: $(rr_child_pids)"
	error_exit 1
fi
exit_test
