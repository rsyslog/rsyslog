#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
action(type="omfile" file="rsyslog.out.log")
'
export RSYSLOG_PRELOAD=.libs/liboverride_gethostname.so
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

grep " localhost " < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "expected hostname \"localhost\" not found in logs, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
