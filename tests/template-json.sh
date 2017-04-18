#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: FIX ME JSON"
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
set $!backslash = "a \\ \"b\" c / d"; # '/' must not be escaped!

template(name="json" type="list" option.json="on") {
        constant(value="{")
        constant(value="\"backslash\":\"")     
		property(name="$!backslash")
        constant(value="\"}\n")
}

:msg, contains, "msgnum:" action(type="omfile" template="json"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

echo '{"backslash":"a \\ \"b\" c / d"}' | cmp rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid JSON generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
