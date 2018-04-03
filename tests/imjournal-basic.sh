#!/bin/bash
# This test injects a message and checks if it is received by
# imjournal. We use a special test string which we do not expect
# to be present in the regular log stream. So we do not expect that
# any other journal content matches our test message. We pull the
# complete journal content for this test, this may be a bit lengthy
# in some environments. But we think that's the only way to check the
# basic core functionality.
# addd 2017-10-25 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh require-journalctl
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imjournal/.libs/imjournal")

template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
TESTMSG="TestBenCH-RSYSLog imjournal This is a test message - $(date +%s)"
./journal_print "$TESTMSG"
./msleep 500
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
journalctl -q | grep -qF "$TESTMSG"
if [ $? -ne 0 ]; then
  echo "FAIL: rsyslog.out.log content (tail -n200):"
  tail -n200 rsyslog.out.log
  echo "======="
  echo "last entries from journal:"
  journalctl -q|tail -n200
  echo "======="
  echo "NOTE: last 200 lines may be insufficient on busy systems!"
  echo "======="
  echo "FAIL: imjournal test message could not be found!"
  echo "Expected message content was:"
  echo "$TESTMSG"
  . $srcdir/diag.sh error-exit 1
fi;
. $srcdir/diag.sh exit
