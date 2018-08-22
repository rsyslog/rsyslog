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
   echo "see https://github.com/rsyslog/rsyslog/issues/2833"
   exit 77
fi

. $srcdir/diag.sh init
generate_conf
add_conf '
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
export RSYSLOG_PRELOAD=".libs/liboverride_gethostname_nonfqdn.so:.libs/liboverride_getaddrinfo.so"
startup
sleep 1
. $srcdir/diag.sh shutdown-immediate
wait_shutdown    # we need to wait until rsyslogd is finished!

grep " nonfqdn " < $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "expected hostname \"nonfqdn\" not found in logs, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

echo EVERYTHING OK - error messages are just as expected!
echo "note: the listener error on port 13500 is OK! It is caused by plumbing"
echo "we do not use here in this special case, and we did not want to work"
echo "very hard to remove that problem (which does not affect the test)"
exit_test
