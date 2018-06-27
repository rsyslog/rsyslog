#!/bin/bash
# add 2018-06-25 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%timestamp:::date-rfc3164%\n")

:syslogtag, contains, "TAG" action(type="omfile" file="rsyslog.out.log"
				   template="outfmt")


'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Jan  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Feb  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Apr  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>May  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Jun  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Jul  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Aug  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Sep  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Oct  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Nov  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Dec  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Jan  6 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<167>Jan 16 16:57:54 172.20.245.8 TAG: MSG\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo 'Jan  6 16:57:54
Feb  6 16:57:54
Mar  6 16:57:54
Apr  6 16:57:54
May  6 16:57:54
Jun  6 16:57:54
Jul  6 16:57:54
Aug  6 16:57:54
Sep  6 16:57:54
Oct  6 16:57:54
Nov  6 16:57:54
Dec  6 16:57:54
Jan  6 16:57:54
Jan 16 16:57:54' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
