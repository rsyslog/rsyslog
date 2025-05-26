#!/bin/bash
# add 2016-12-08 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmnormalize/.libs/pmnormalize")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset")
parser(name="custom.pmnormalize" type="pmnormalize" rulebase="'$srcdir'/testsuites/pmnormalize_basic.rulebase")

template(name="test" type="string" string="host: %hostname%, ip: %fromhost-ip%, tag: %syslogtag%, pri: %pri%, syslogfacility: %syslogfacility%, syslogseverity: %syslogseverity% msg: %msg%\n")

ruleset(name="ruleset" parser="custom.pmnormalize") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="test")
}
'
startup
tcpflood -m1 -M "\"<189> ubuntu tag1: is no longer listening on 127.0.0.1 test\""
tcpflood -m1 -M "\"<112> debian tag2: is no longer listening on 255.255.255.255 test\""
tcpflood -m1 -M "\"<177> centos tag3: is no longer listening on 192.168.0.9 test\""
shutdown_when_empty
wait_shutdown
sort < $RSYSLOG_OUT_LOG > ${RSYSLOG_OUT_LOG}.sorted
export EXPECTED='host: centos, ip: 192.168.0.9, tag: tag3, pri: 177, syslogfacility: 22, syslogseverity: 1 msg: test
host: debian, ip: 255.255.255.255, tag: tag2, pri: 112, syslogfacility: 14, syslogseverity: 0 msg: test
host: ubuntu, ip: 127.0.0.1, tag: tag1, pri: 189, syslogfacility: 23, syslogseverity: 5 msg: test'
cmp_exact ${RSYSLOG_OUT_LOG}.sorted

exit_test
