#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: the configured imjournal state file path exists but is
# not a regular file. By default it is a directory (for example left over
# after removing input() instances, which turn the state file path into a
# directory). Wrappers reuse this scenario:
#   IMJOURNAL_STATEFILE_KIND=fifo  the path is a FIFO without a writer
#                                  (imjournal-statefile-fifo.sh)
#   IMJOURNAL_TEST_YAML=1          imjournal module and its parameters
#                                  configured in an included YAML file
#                                  (yaml-imjournal-statefile-directory.sh)
# With the default IgnoreNonValidStatefile="on", imjournal must log the
# error, ignore the state file, keep reading the journal and must neither
# block on the FIFO nor replace the path when it saves its position.
# Previously the input thread terminated (directory, FIFO with a writer) or
# blocked in open() (FIFO without a writer) and nothing was read.
# Oracle: the test message injected via journal_print is written to the
# output file exactly once, checked while running and again after shutdown
# (so a duplicate written late also fails), and the state path still has
# its original type after shutdown. The test is skipped (77) if the journal
# cannot be read or the message does not show up in it within 30 seconds;
# the skip pre-check polls once per second and stops as soon as the message
# is visible. The oracle itself polls for up to 300 seconds.
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh require-journalctl
STATEFILE_KIND=${IMJOURNAL_STATEFILE_KIND:-directory}
STATEFILE="$RSYSLOG_DYNNAME.spool/imjournal.state"

if [ "${IMJOURNAL_TEST_YAML:-0}" = 1 ]; then
	# the imjournal module and its parameters come from a YAML file; the
	# default ruleset cannot be extended from YAML, so the filter and
	# action stay in RainerScript (same layout as yaml-imfile-basic.sh)
	require_yaml_support
	IMJOURNAL_MODULE_CONF='include(file="'"$RSYSLOG_DYNNAME"'.yaml")'
	cat > "$RSYSLOG_DYNNAME.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imjournal/.libs/imjournal"
    statefile: "imjournal.state"
    ratelimit.interval: "0"
YAMLEOF
else
	IMJOURNAL_MODULE_CONF='module(load="../plugins/imjournal/.libs/imjournal" StateFile="imjournal.state"
	# we turn off rate-limiting, else we may miss our test message:
	RateLimit.interval="0")'
fi

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
'"$IMJOURNAL_MODULE_CONF"'

template(name="outfmt" type="string" string="%msg%\n")

# Filter to only process messages from this test instance to avoid interference
# from other parallel journal tests writing to the same system journal
if $msg contains "'"$RSYSLOG_DYNNAME"'" then {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
	stop
}
'

# pre-create the state file path with the requested type
case "$STATEFILE_KIND" in
directory)
	mkdir -p "$STATEFILE"
	STATEFILE_TEST="-d"
	;;
fifo)
	mkfifo "$STATEFILE"
	STATEFILE_TEST="-p"
	;;
*)
	echo "FAIL: unknown IMJOURNAL_STATEFILE_KIND '$STATEFILE_KIND'"
	error_exit 1
	;;
esac

TESTMSG="TestBenCH-RSYSLog imjournal statefile $STATEFILE_KIND test - $(date +%s) - $RSYSLOG_DYNNAME"
./journal_print "$TESTMSG"
if [ $? -ne 0 ]; then
	echo "SKIP: failed to put test into journal."
	error_exit 77
fi

journalctl -an 200 > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "SKIP: cannot read journal."
	error_exit 77
fi

# journald stores the message asynchronously; wait until it is visible
found=0
for _ in $(seq 1 30); do
	if journalctl -an 200 | grep -qF "$TESTMSG"; then
		found=1
		break
	fi
	sleep 1
done
if [ "$found" -ne 1 ]; then
	echo "SKIP: cannot find '$TESTMSG' in journal."
	error_exit 77
fi

startup

# give the journal ~5 minutes to forward the message, see
# https://github.com/rsyslog/rsyslog/issues/2564#issuecomment-435849660
content_check_with_count "$TESTMSG" 1 300

shutdown_when_empty
wait_shutdown

# re-count after shutdown: a duplicate written late must fail the test
content_count_check "$TESTMSG" 1

if ! test "$STATEFILE_TEST" "$STATEFILE"; then
	echo "FAIL: state file path ($STATEFILE_KIND) was unexpectedly replaced"
	ls -l "$STATEFILE"
	error_exit 1
fi
exit_test
