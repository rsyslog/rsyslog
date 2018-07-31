#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="+%syslogtag:1:32%+\n")

:pri, contains, "167" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
				   template="outfmt")


'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 TAG: Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 0 Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901 Rest of message...\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901-toolong Rest of message...\""
shutdown_when_empty
wait_shutdown

echo '+TAG:+
+0+
+01234567890123456789012345678901+
+01234567890123456789012345678901+' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit  1
fi;

exit_test
