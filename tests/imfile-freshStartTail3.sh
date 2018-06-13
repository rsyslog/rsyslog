#!/bin/bash
# add 2018-05-17 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" freshStartTail="on" Tag="pro"
	File="rsyslog.input")

template(name="outfmt" type="string" string="%msg%\n")

:syslogtag, contains, "pro" action(type="omfile" File="rsyslog.out.log"
	template="outfmt")
'

echo '{ "id": "jinqiao1"}' > rsyslog.input
. $srcdir/diag.sh startup
./msleep 2000
echo '{ "id": "jinqiao2"}' >> rsyslog.input

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '{ "id": "jinqiao2"}' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
