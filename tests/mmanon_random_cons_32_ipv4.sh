#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg%\n")
template(name="filename" type="string" string="rsyslog.out.%syslogtag%.log")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.mode="random-consistent" ipv4.bits="32")
	action(type="omfile" dynafile="filename" template="outfmt")
}'

echo 'Since this test tests randomization, there is a theoretical possibility of it failing even if rsyslog works correctly. Therefore, if the test unexpectedly fails try restarting it.'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 file1 1.1.1.8
<129>Mar 10 01:00:00 172.20.245.8 file2 0.0.0.0
<129>Mar 10 01:00:00 172.20.245.8 file3 0.0.0.0
<129>Mar 10 01:00:00 172.20.245.8 file4 172.0.234.255
<129>Mar 10 01:00:00 172.20.245.8 file5 172.1.234.255
<129>Mar 10 01:00:00 172.20.245.8 file6 1.1.1.8
<129>Mar 10 01:00:00 172.20.245.8 file7 111.1.1.8.
<129>Mar 10 01:00:00 172.20.245.8 file8 1.1.1.8\""

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo ' 1.1.1.8' | cmp rsyslog.out.file1.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file1.log is:"
  cat rsyslog.out.file1.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' 0.0.0.0' | cmp rsyslog.out.file2.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file2.log is:"
  cat rsyslog.out.file2.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' 172.0.234.255' | cmp rsyslog.out.file4.log  >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file4.log is:"
  cat rsyslog.out.file4.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' 111.1.1.8.' | cmp rsyslog.out.file7.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file7.log is:"
  cat rsyslog.out.file7.log
  . $srcdir/diag.sh error-exit  1
fi;

cmp rsyslog.out.file1.log rsyslog.out.file6.log >/dev/null
if [ ! $? -eq 0 ]; then
  echo "invalidly unequal ip-addresses generated, rsyslog.out.file1.log and rsyslog.out.file6.log are:"
  cat rsyslog.out.file1.log
  cat rsyslog.out.file6.log
  . $srcdir/diag.sh error-exit  1
fi;

cmp rsyslog.out.file6.log rsyslog.out.file8.log >/dev/null
if [ ! $? -eq 0 ]; then
  echo "invalidly unequal ip-addresses generated, rsyslog.out.file6.log and rsyslog.out.file8.log are:"
  cat rsyslog.out.file6.log
  cat rsyslog.out.file8.log
  . $srcdir/diag.sh error-exit  1
fi;

cmp rsyslog.out.file2.log rsyslog.out.file3.log >/dev/null
if [ ! $? -eq 0 ]; then
  echo "invalidly unequal ip-addresses generated, rsyslog.out.file2.log and rsyslog.out.file3.log are:"
  cat rsyslog.out.file2.log
  cat rsyslog.out.file3.log
  . $srcdir/diag.sh error-exit  1
fi;

cmp rsyslog.out.file4.log rsyslog.out.file5.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-addresses generated, rsyslog.out.file4.log and rsyslog.out.file5.log are:"
  cat rsyslog.out.file4.log
  cat rsyslog.out.file5.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
