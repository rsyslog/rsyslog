#!/bin/bash
# add 2018-06-25 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%timestamp:::date-rfc3339%\n")

:syslogtag, contains, "su" action(type="omfile" file="rsyslog.out.log"
				  template="outfmt")

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T22:14:15.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-01-11T22:14:15.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-01T22:04:15.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T02:14:15.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T22:04:05.003Z mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T22:04:05.003+02:00 mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T22:04:05.003+01:30 mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<34>1 2003-11-11T22:04:05.123456+01:30 mymachine.example.com su - ID47 - MSG\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '2003-11-11T22:14:15.003Z
2003-01-11T22:14:15.003Z
2003-11-01T22:04:15.003Z
2003-11-11T02:14:15.003Z
2003-11-11T22:04:05.003Z
2003-11-11T22:04:05.003+02:00
2003-11-11T22:04:05.003+01:30
2003-11-11T22:04:05.123456+01:30' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
