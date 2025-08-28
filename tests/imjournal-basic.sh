#!/bin/bash
# This test injects a message and checks if it is received by
# imjournal. We use a special test string which we do not expect
# to be present in the regular log stream. So we do not expect that
# any other journal content matches our test message. We skip the 
# test in case message does not make it even to journal which may 
# sometimes happen in some environments.
# add 2017-10-25 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf
add_conf '
module(load="../plugins/imjournal/.libs/imjournal" IgnorePreviousMessages="on"
	RateLimit.Burst="1000000")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
TESTMSG="TestBenCH-RSYSLog imjournal This is a test message - $(date +%s) - $RSYSLOG_DYNNAME"

startup

printf 'a quick glimpse at journal content at rsyslog startup:\n'
journalctl -n 20 --no-pager
printf '\n\n'

printf '++++++++++++++++++++++ Printing to the journal! +++++++++++++++++++++++++\n'
# inject message into journal and check that it is recorded
./journal_print "$TESTMSG"
journal_write_state=$?
if [ $journal_write_state -ne 0 ]; then
	printf 'SKIP: journal_print returned state %d writing message: %s\n' "$journal_write_state" "$TESTMSG"
	printf 'skipping test, journal probably not working\n'
	exit 77
fi

# check state later - we must not terminate the test until we have terminated rsyslog

# give the journal ~5 minutes to forward the message, see
# https://github.com/rsyslog/rsyslog/issues/2564#issuecomment-435849660
content_check_with_count "$TESTMSG" 1 300

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown

check_journal_testmsg_received
exit_test
