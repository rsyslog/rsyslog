#!/bin/bash
# This tests checks for a anomaly we have seen in practice:
# gethostname() may return an empty string as hostname (""). This broke
# some versions of rsyslog, newer ones return "localhost" in that case.
# The test is done with the help of a preload library specifically written
# for this purpose (liboverride_gethostname.so). It will override
# gethostname() and return an empty string. Then, the test checks if the
# hardcoded default of "localhost-empty-hostname" is used.
# Note that the test may fail if the library is not properly preloaded.
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
. $srcdir/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="rsyslog.out.log")
'
export RSYSLOG_PRELOAD=.libs/liboverride_gethostname.so
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown    # we need to wait until rsyslogd is finished!

grep " localhost-empty-hostname " < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "expected hostname \"localhost-empty-hostname\" not found in logs, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit 1
fi;

exit_test
