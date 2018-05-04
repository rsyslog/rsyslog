#!/bin/bash
# This test check what happens if we cannot doe getaddrinfo early
# in rsyslog startup  (this has caused an error in the past). Even more
# importantly, it checks that error messages can be issued very early
# during startup.
# Note that we use the override of the hostname to ensure we do not
# accidentely get an acceptable FQDN-type hostname during testing.
# This is part of the rsyslog testbench, licensed under ASL 2.0
uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris: there seems to be an issue with LD_PRELOAD libraries"
   exit 77
fi
if [ `uname` = "FreeBSD" ] ; then
   echo "FreeBSD: temporarily disabled until we know what is wrong"
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
action(type="omfile" file="rsyslog.out.log")
'
export RSYSLOG_PRELOAD=".libs/liboverride_gethostname_nonfqdn.so:.libs/liboverride_getaddrinfo.so"
. $srcdir/diag.sh startup
sleep 1
. $srcdir/diag.sh shutdown-immediate
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

grep " nonfqdn " < rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "expected hostname \"nonfqdn\" not found in logs, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

echo EVERYTHING OK - error messages are just as expected!
echo "note: the listener error on port 13500 is OK! It is caused by plumbing"
echo "we do not use here in this special case, and we did not want to work"
echo "very hard to remove that problem (which does not affect the test)"
. $srcdir/diag.sh exit
