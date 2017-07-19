#!/bin/bash
# add 2017-07-18 by Pascal Withopf, released under ASL 2.0
echo \[imklog_permitnonkernelfacility_root.sh\]: test parameter permitnonkernelfacility
echo This test must be run as root with no other active syslogd
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
service rsyslog stop
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imklog/.libs/imklog" permitnonkernelfacility="on")

template(name="outfmt" type="string" string="%msg:57:16%: -%pri%-\n")

:msg, contains, "msgnum" action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
echo "<115>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1" > /dev/kmsg
echo "<115>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1"
sleep 2
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo 'Mar 10 01:00:00 172.20.245.8 tag: msgnum:1: -115-' | cmp rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

service rsyslog start
. $srcdir/diag.sh exit
