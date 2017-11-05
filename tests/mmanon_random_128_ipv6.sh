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
	action(type="mmanon" ipv6.anonmode="random" ipv6.bits="128" ipv6.enable="on")
	action(type="omfile" dynafile="filename" template="outfmt")
}'

echo 'Since this test tests randomization, there is a theoretical possibility of it failing even if rsyslog works correctly. Therefore, if the test unexpectedly fails try restarting it.'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 file1 33:45:DDD::4
<129>Mar 10 01:00:00 172.20.245.8 file2 ::
<129>Mar 10 01:00:00 172.20.245.8 file3 72:8374:adc7:47FF::43:0:1AFE
<129>Mar 10 01:00:00 172.20.245.8 file4 FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
<129>Mar 10 01:00:00 172.20.245.8 file5 72:8374:adc7:47FF::43:0:1AFE\""

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo ' 33:45:DDD::4' | cmp rsyslog.out.file1.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file1.log is:"
  cat rsyslog.out.file1.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' ::' | cmp rsyslog.out.file2.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file2.log is:"
  cat rsyslog.out.file2.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' 72:8374:adc7:47FF::43:0:1AFE' | cmp rsyslog.out.file3.log  >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file3.log is:"
  cat rsyslog.out.file3.log
  . $srcdir/diag.sh error-exit  1
fi;

echo ' FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF' | cmp rsyslog.out.file4.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-address generated, rsyslog.out.file4.log is:"
  cat rsyslog.out.file4.log
  . $srcdir/diag.sh error-exit  1
fi;

cmp rsyslog.out.file3.log rsyslog.out.file5.log >/dev/null
if [ $? -eq 0 ]; then
  echo "invalidly equal ip-addresses generated, rsyslog.out.file3.log and rsyslog.out.file5.log are:"
  cat rsyslog.out.file3.log
  cat rsyslog.out.file5.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
