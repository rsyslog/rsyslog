#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09 by Rainer Gerhards, released under ASL 2.0
#
# Intent: imbatchreport must not read past its 150-byte structured-data tail
# buffer when the trailing [...] block does not contain the configured
# timestampkey/programkey. The tail is read from the file without a NUL
# terminator; before the fix, strstr() searched for the missing keys beyond
# the end of the stack buffer (detected by ASan as stack-buffer-overflow, and
# undefined behavior in normal builds).
#
# Setup: a report whose last 150+ bytes contain no NUL and end with a
# structured-data block that has neither START= nor KSH=. Its mtime is set to
# a fixed time so the fallback timestamp is deterministic.
# Oracle: the report is consumed (deleted) and exactly one message is emitted
# with the file's structured data, the mtime-derived timestamp and the body,
# showing the no-key path completes normally.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(maxmessagesize="820")
module(load="../contrib/imbatchreport/.libs/imbatchreport" pollinginterval="1")
global(localhostname="server")
input(type="imbatchreport" ruleset="ruleset" tag="batch"
      severity="info" facility="local0"
      reports="./'$RSYSLOG_DYNNAME'*.done" deduplicatespace="on"
      programkey="KSH" timestampkey="START"
      delete=".done$ 	.rejected"
      )
ruleset(name="ruleset") {
   action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="RSYSLOG_SyslogProtocol23Format")
}
'
{
 echo 'line one of the batch report body, long enough to fill the tail window aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
 echo 'line two of the batch report body bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
 echo '[OTHER=1234567890 NAME="nokeys" DURATION=120]'
} > $RSYSLOG_DYNNAME.nk.done
touch -d @1552143924 $RSYSLOG_DYNNAME.nk.done
startup
shutdown_when_empty
wait_shutdown
export EXPECTED='<134>1 2019-03-09T15:05:24.000000+00:00 server batch - - [OTHER=1234567890 NAME="nokeys" DURATION=120] line one of the batch report body, long enough to fill the tail window aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nline two of the batch report body bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'
cmp_exact
if [ -e $RSYSLOG_DYNNAME.nk.done ]; then
  echo "The batch report was not consumed"
  ls $RSYSLOG_DYNNAME*
  error_exit 1
fi
exit_test
