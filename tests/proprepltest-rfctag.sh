#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="+%syslogtag:1:32%+\n")

:pri, contains, "167" action(type="omfile" file="rsyslog.out.log"
				   template="outfmt")


'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 TAG: Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 0 Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901 Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901-toolong Rest of message...\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '+TAG:+
+0+
+01234567890123456789012345678901+
+01234567890123456789012345678901+' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
