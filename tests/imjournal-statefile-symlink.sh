#!/bin/bash
# Ensure imjournal does not follow or reuse attacker-controlled temp state files.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
generate_conf

VICTIM="$RSYSLOG_DYNNAME.spool/victim.txt"
STATEFILE_BASE="$RSYSLOG_DYNNAME.spool/imjournal.state"

printf '%s\n' "do-not-touch" > "$VICTIM"
ln -s victim.txt "$STATEFILE_BASE.tmp"

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	RateLimit.interval="0")

template(name="outfmt" type="string" string="%msg%\n")

if $msg contains "'"$RSYSLOG_DYNNAME"'" then {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
	stop
}
'

TESTMSG="TestBenCH-RSYSLog imjournal symlink statefile test - $(date +%s) - $RSYSLOG_DYNNAME"
./journal_print "$TESTMSG"
if [ $? -ne 0 ]; then
	echo "SKIP: failed to put test into journal."
	error_exit 77
fi

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
content_check_with_count "$TESTMSG" 1 300
shutdown_when_empty
wait_shutdown

export EXPECTED='do-not-touch'
cmp_exact "$VICTIM"

if [ ! -f "$STATEFILE_BASE" ]; then
	echo "FAIL: expected state file '$STATEFILE_BASE' was not created"
	error_exit 1
fi

if [ -L "$STATEFILE_BASE" ]; then
	echo "FAIL: state file '$STATEFILE_BASE' must not be a symlink"
	error_exit 1
fi

exit_test
