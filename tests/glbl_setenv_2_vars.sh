#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(environment=["http_proxy=http://127.0.0.1", "SECOND=OK OK"])

set $!prx = getenv("http_proxy");
set $!second = getenv("SECOND");

template(name="outfmt" type="string" string="%$!prx%, %$!second%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

echo 'http://127.0.0.1, OK OK' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid content seen, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
