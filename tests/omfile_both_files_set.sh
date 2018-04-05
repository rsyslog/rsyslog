#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="dynafile" type="string" string="rsyslog.out.log")
template(name="outfmt" type="string" string="-%msg%-\n")

:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file="rsyslog2.out.log" dynafile="dynafile")
}
action(type="omfile" file="rsyslog.errorfile") 
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "will use dynafile" rsyslog.errorfile > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.errorfile is:"
	cat rsyslog.errorfile
	. $srcdir/diag.sh error-exit 1
fi

echo '- msgnum:1-' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "unexpected content in rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

if [ -f rsyslog2.out.log ]; then
  echo "file exists, but should not: rsyslog2.out.log; content:"
  cat rsyslog2.out.log
  . $srcdir/diag.sh error-exit  1
fi;


. $srcdir/diag.sh exit
