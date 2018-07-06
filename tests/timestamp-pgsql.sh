#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%timestamp:::date-pgsql%\n")

:syslogtag, contains, "su" action(type="omfile" file="rsyslog.out.log"
				   template="outfmt")


'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-01-23T12:34:56.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '2003-01-23 12:34:56' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
