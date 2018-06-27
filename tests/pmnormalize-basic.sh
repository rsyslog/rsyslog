#!/bin/bash
# add 2016-12-08 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmnormalize/.libs/pmnormalize")

input(type="imtcp" port="13514" ruleset="ruleset")
parser(name="custom.pmnormalize" type="pmnormalize" rulebase=`echo $srcdir/testsuites/pmnormalize_basic.rulebase`)

template(name="test" type="string" string="host: %hostname%, ip: %fromhost-ip%, tag: %syslogtag%, pri: %pri%, syslogfacility: %syslogfacility%, syslogseverity: %syslogseverity% msg: %msg%\n")

ruleset(name="ruleset" parser="custom.pmnormalize") {
	action(type="omfile" file="rsyslog.out.log" template="test")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<189> ubuntu tag1: is no longer listening on 127.0.0.1 test\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<112> debian tag2: is no longer listening on 255.255.255.255 test\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<177> centos tag3: is no longer listening on 192.168.0.9 test\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo 'host: ubuntu, ip: 127.0.0.1, tag: tag1, pri: 189, syslogfacility: 23, syslogseverity: 5 msg: test
host: debian, ip: 255.255.255.255, tag: tag2, pri: 112, syslogfacility: 14, syslogseverity: 0 msg: test
host: centos, ip: 192.168.0.9, tag: tag3, pri: 177, syslogfacility: 22, syslogseverity: 1 msg: test' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
