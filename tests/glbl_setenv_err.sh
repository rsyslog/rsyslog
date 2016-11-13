#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
# env var is missing equal sign and MUST trigger parsing error!
global(environment="http_proxy ERROR")

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

grep "http_proxy ERROR" < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo 
  echo "MESSAGE INDICATING ERROR ON ENVIRONMENT VARIABLE IS MISSING:"
  echo 
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
