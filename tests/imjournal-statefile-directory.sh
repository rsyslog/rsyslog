#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: the configured imjournal state file path exists but is
# a directory (for example left over after removing input() instances,
# which turn the state file path into a directory). With the default
# IgnoreNonValidStatefile="on", imjournal must log the error, ignore the
# state file and keep reading the journal. Previously the input thread
# terminated and nothing was read.
# Oracle: the test message injected via journal_print is written to the
# output file exactly once. The test is skipped (77) if the message does
# not reach the journal or the journal cannot be read. The short sleep
# only lets journald store the message before the journalctl skip
# pre-check; the oracle itself polls for up to 300 seconds.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf

# pre-create the state file path as a directory
mkdir -p "$RSYSLOG_DYNNAME.spool/imjournal.state"

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	# we turn off rate-limiting, else we may miss our test message:
	RateLimit.interval="0")

template(name="outfmt" type="string" string="%msg%\n")

# Filter to only process messages from this test instance to avoid interference
# from other parallel journal tests writing to the same system journal
if $msg contains "'"$RSYSLOG_DYNNAME"'" then {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
	stop
}
'
TESTMSG="TestBenCH-RSYSLog imjournal statefile directory test - $(date +%s) - $RSYSLOG_DYNNAME"
./journal_print "$TESTMSG"
if [ $? -ne 0 ]; then
	echo "SKIP: failed to put test into journal."
	error_exit 77
fi

# sleep 1 to get this test to reliably detect the message
sleep 1

journalctl -an 200 > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "SKIP: cannot read journal."
	error_exit 77
fi

journalctl -an 200 | grep -qF "$TESTMSG"
if [ $? -ne 0 ]; then
	echo "SKIP: cannot find '$TESTMSG' in journal."
	error_exit 77
fi

startup

# give the journal ~5 minutes to forward the message, see
# https://github.com/rsyslog/rsyslog/issues/2564#issuecomment-435849660
content_check_with_count "$TESTMSG" 1 300

shutdown_when_empty
wait_shutdown

if [ ! -d "$RSYSLOG_DYNNAME.spool/imjournal.state" ]; then
	echo "FAIL: state file directory was unexpectedly replaced"
	error_exit 1
fi
exit_test
