#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
#
# Intent: issue_HUP must detect HUP completion exactly instead of by timing.
# rsyslogd counts fully processed HUPs; imdiag reports the count
# ("GetHUPCount") and waits for it to change ("AwaitHUPCount <old> [ms]").
#
# Oracle:
# - the count starts at 0 and is 2 after two issue_HUP calls (each call
#   returns only after its HUP finished, so the count is exact, not racy);
# - with no HUP pending, "AwaitHUPCount <current> 200" must report a timeout
#   error after about 200 ms instead of claiming success;
# - a missing or non-numeric count must be rejected with an error.
# The 200 ms timeout only bounds the negative case; no success depends on it.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

ask() {
	echo "$1" | $TESTTOOL_DIR/diagtalker -p"$IMDIAG_PORT" || error_exit $?
}

reply=$(ask GetHUPCount)
if [ "$reply" != "imdiag[$IMDIAG_PORT]: 0" ]; then
	echo "FAIL: initial HUP count wrong: '$reply'"
	error_exit 1
fi

issue_HUP
issue_HUP

reply=$(ask GetHUPCount)
if [ "$reply" != "imdiag[$IMDIAG_PORT]: 2" ]; then
	echo "FAIL: HUP count after two HUPs wrong: '$reply'"
	error_exit 1
fi

reply=$(ask "AwaitHUPCount 2 200")
case "$reply" in
*"ERROR: timeout"*) ;;
*)
	echo "FAIL: AwaitHUPCount without pending HUP did not time out: '$reply'"
	error_exit 1
	;;
esac

for bad in "AwaitHUPCount" "AwaitHUPCount abc" "AwaitHUPCount 1 0"; do
	reply=$(ask "$bad")
	case "$reply" in
	*"ERROR:"*) ;;
	*)
		echo "FAIL: '$bad' was not rejected: '$reply'"
		error_exit 1
		;;
	esac
done

shutdown_when_empty
wait_shutdown
exit_test
