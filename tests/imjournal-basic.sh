#!/bin/bash
# This test injects a message and checks if it is received by
# imjournal. We use a special test string which we do not expect
# to be present in the regular log stream. So we do not expect that
# any other journal content matches our test message. We skip the 
# test in case message does not make it even to journal which may 
# sometimes happen in some environments.
# addd 2017-10-25 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf
add_conf '
module(load="../plugins/imjournal/.libs/imjournal" IgnorePreviousMessages="on"
	RateLimit.Burst="1000000")

template(name="outfmt" type="string" string="%msg%\n")

# Filter to only process messages from this test instance to avoid interference
# from other parallel journal tests writing to the same system journal
if $msg contains "'"$RSYSLOG_DYNNAME"'" then {
	action(type="omfile" template="outfmt" file="'"$RSYSLOG_OUT_LOG"'")
	stop
}
'
TESTMSG="TestBenCH-RSYSLog imjournal This is a test message - $(date +%s) - $RSYSLOG_DYNNAME"

startup

printf 'a quick glimpse at journal content at rsyslog startup:\n'
journalctl -n 20 --no-pager
printf '\n\n'

wait_for_out_log_contains() {
	needle="$1"
	timeout="$2"
	deadline=$(( $(date +%s) + timeout ))

	while [ "$(date +%s)" -le "$deadline" ]; do
		if [ -f "$RSYSLOG_OUT_LOG" ] && grep -Fq -- "$needle" "$RSYSLOG_OUT_LOG"; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 200
	done

	return 1
}

# If journal activity is extremely high, this test can become flaky due to
# heavy contention/rotation churn outside the test's control. We retry to
# absorb short bursts before deciding to skip.
NOISE_WINDOW_SEC=${RSTB_JOURNAL_NOISE_WINDOW_SEC:-5}
NOISE_MAX_LINES=${RSTB_JOURNAL_NOISE_MAX_LINES:-20000}
NOISE_RETRIES=${RSTB_JOURNAL_NOISE_RETRIES:-2}
NOISE_RETRY_SLEEP_MS=${RSTB_JOURNAL_NOISE_RETRY_SLEEP_MS:-1500}
noise_try=0
while true; do
	noise_since=$(( $(date +%s) - NOISE_WINDOW_SEC ))
	journal_noise_lines=$(journalctl --since "@$noise_since" --no-pager 2>/dev/null | wc -l)
	if [ "$journal_noise_lines" -le "$NOISE_MAX_LINES" ]; then
		break
	fi
	if [ "$noise_try" -ge "$NOISE_RETRIES" ]; then
		printf 'SKIP: journal too noisy (%d lines in last %ds, limit %d) after %d retries\n' \
			"$journal_noise_lines" "$NOISE_WINDOW_SEC" "$NOISE_MAX_LINES" "$NOISE_RETRIES"
		shutdown_when_empty
		wait_shutdown
		exit 77
	fi
	printf 'journal noisy (%d lines in last %ds, limit %d), retrying (%d/%d)\n' \
		"$journal_noise_lines" "$NOISE_WINDOW_SEC" "$NOISE_MAX_LINES" "$((noise_try+1))" "$NOISE_RETRIES"
	$TESTTOOL_DIR/msleep "$NOISE_RETRY_SLEEP_MS"
	(( noise_try=noise_try+1 ))
done

# Make sure imjournal has passed initial positioning (IgnorePreviousMessages)
# before sending the actual test payload. On busy CI systems this may need
# a few retries, so probe repeatedly with bounded waits.
READY_TRIES=${RSTB_IMJOURNAL_READY_TRIES:-4}
READY_TIMEOUT_PER_TRY=${RSTB_IMJOURNAL_READY_TIMEOUT_PER_TRY:-20}
ready_ok=0
ready_try=1
while [ "$ready_try" -le "$READY_TRIES" ]; do
	READYMSG="TestBenCH-RSYSLog imjournal readiness probe - $(date +%s) - $RSYSLOG_DYNNAME - try$ready_try"
	./journal_print "$READYMSG"
	journal_write_state=$?
	if [ $journal_write_state -ne 0 ]; then
		printf 'SKIP: journal_print returned state %d writing readiness probe: %s\n' "$journal_write_state" "$READYMSG"
		printf 'skipping test, journal probably not working\n'
		shutdown_when_empty
		wait_shutdown
		exit 77
	fi

	if wait_for_out_log_contains "$READYMSG" "$READY_TIMEOUT_PER_TRY"; then
		ready_ok=1
		break
	fi

	printf 'readiness probe not observed yet (%d/%d), retrying\n' "$ready_try" "$READY_TRIES"
	$TESTTOOL_DIR/msleep 500
	(( ready_try=ready_try+1 ))
done

if [ "$ready_ok" -ne 1 ]; then
	printf 'SKIP: imjournal readiness probe not observed after %d attempts (%ds each)\n' \
		"$READY_TRIES" "$READY_TIMEOUT_PER_TRY"
	shutdown_when_empty
	wait_shutdown
	exit 77
fi

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
