#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: imjournal with StateFile set and PersistStateInterval="0".
# A value of 0 disables periodic persisting of the journal cursor. Older
# versions used the value as a modulus, i.e. divided by zero on the first
# journal entry read (undefined behavior; SIGFPE on x86).
#
# The imjournal module parameters are set via RainerScript (default) or,
# when IMJOURNAL_PSI0_YAML=1 is set (see
# yaml-imjournal-persiststateinterval-zero.sh), via an included YAML file
# that sets the same parameters.
#
# Oracle: the injected test message is received exactly once, rsyslogd
# shuts down cleanly (wait_shutdown fails on abnormal termination), and the
# state file exists and is non-empty afterwards, because the cursor is still
# written at shutdown even if periodic persisting is disabled.
#
# Journal readiness: journald may take a moment to expose the injected
# message. The test polls journalctl once per second for up to 30 seconds
# and continues as soon as the message is visible. Only if the journal
# cannot be read, or the message is still not visible after that bound, the
# test skips (77), like the other imjournal tests.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
if [ "${IMJOURNAL_PSI0_YAML:-0}" = 1 ]; then
	require_yaml_support
fi

STATEFILE="$RSYSLOG_DYNNAME.spool/imjournal.state"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
'
if [ "${IMJOURNAL_PSI0_YAML:-0}" = 1 ]; then
	# Same module parameters, but parsed by the YAML loader. The filter and
	# action stay in RainerScript (as in yaml-imdocker-defaultfacility.sh)
	# because imjournal's module-level listener feeds the default ruleset,
	# and a YAML rulesets: entry of that name is rejected as a duplicate.
	cat > "$RSYSLOG_DYNNAME.imjournal.yaml" <<'YAMLEOF'
version: 2
modules:
  - load: "../plugins/imjournal/.libs/imjournal"
    statefile: "imjournal.state"
    persiststateinterval: "0"
    # we turn off rate-limiting, else we may miss our test message:
    ratelimit.interval: "0"
YAMLEOF
	add_conf '
include(file="'$RSYSLOG_DYNNAME'.imjournal.yaml")
'
else
	add_conf '
module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	PersistStateInterval="0"
	# we turn off rate-limiting, else we may miss our test message:
	RateLimit.interval="0")
'
fi
add_conf '
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

# bounded readiness poll: leave as soon as the message is visible
msg_found=0
for _ in $(seq 1 30); do
	if ! journalctl -an 200 > /dev/null 2>&1; then
		echo "SKIP: cannot read journal."
		error_exit 77
	fi
	if journalctl -an 200 | grep -qF "$TESTMSG"; then
		msg_found=1
		break
	fi
	sleep 1
done
if [ "$msg_found" -ne 1 ]; then
	echo "SKIP: cannot find '$TESTMSG' in journal after 30 seconds."
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
