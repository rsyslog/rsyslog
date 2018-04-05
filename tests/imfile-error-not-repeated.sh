#!/bin/bash
# add 2017-04-28 by Pascal Withopf, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile" mode="polling" pollingInterval="1")
input(type="imfile" File="./rsyslog.input" Tag="tag1" ruleset="ruleset1")

template(name="tmpl1" type="string" string="%msg%\n")
ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log" template="tmpl1")
}
action(type="omfile" file="rsyslog2.out.log")
'
. $srcdir/diag.sh startup
./msleep 3000

echo 'testmessage1
testmessage2
testmessage3' > rsyslog.input

./msleep 2000
rm ./rsyslog.input
./msleep 3000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "file.*rsyslog.input.*No such file or directory" rsyslog2.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo "FAIL: expected error message from missing input file not found. rsyslog2.out.log is:"
        cat rsyslog2.out.log
        . $srcdir/diag.sh error-exit 1
fi

if [ `grep "No such file or directory" rsyslog2.out.log | wc -l` -ne 1 ]; then
	echo "FAIL: expected error message is put out multiple times. rsyslog2.out.log is:"
	cat rsyslog2.out.log
	. $srcdir/diag.sh error-exit 1
fi

echo 'testmessage1
testmessage2
testmessage3' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
