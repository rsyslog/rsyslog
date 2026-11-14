#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: imjournal with StateFile set and PersistStateInterval="0".
# A value of 0 disables periodic persisting of the journal cursor. Older
# versions used the value as a modulus, i.e. divided by zero on the first
# journal entry read (undefined behavior; SIGFPE on x86).
#
# Oracle: the injected test message is received exactly once, rsyslogd
# shuts down cleanly (wait_shutdown fails on abnormal termination), and the
# state file exists and is non-empty afterwards, because the cursor is still
# written at shutdown even if periodic persisting is disabled.
# The test skips (77) if the message cannot be put into or read back from
# the journal, like the other imjournal tests.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf

STATEFILE="$RSYSLOG_DYNNAME.spool/imjournal.state"

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	PersistStateInterval="0"
	# we turn off rate-limiting, else we may miss our test message:
	RateLimit.interval="0")

template(name="outfmt" type="string" string="%msg%\n")

# only process messages from this test instance to avoid interference
# from other parallel journal tests writing to the same system journal
if $msg contains "'"$RSYSLOG_DYNNAME"'" then {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
	stop
}
'

TESTMSG="TestBenCH-RSYSLog imjournal persiststateinterval zero - $(date +%s) - $RSYSLOG_DYNNAME"
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

if [ ! -s "$STATEFILE" ]; then
	echo "FAIL: state file '$STATEFILE' was not written at shutdown"
	error_exit 1
fi

exit_test
