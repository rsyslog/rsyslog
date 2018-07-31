#!/bin/bash
# add 2017-06-12 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
startup_vg_noleak pmnormalize-rule-vg.conf
. $srcdir/diag.sh tcpflood -m1 -M "\"<189> 127.0.0.1 ubuntu tag1: this is a test message\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<112> 255.255.255.255 debian tag2: this is a test message\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<177> centos 192.168.0.9 tag3: this is a test message\""
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
echo 'host: ubuntu, ip: 127.0.0.1, tag: tag1, pri: 189, syslogfacility: 23, syslogseverity: 5 msg: this is a test message
host: debian, ip: 255.255.255.255, tag: tag2, pri: 112, syslogfacility: 14, syslogseverity: 0 msg: this is a test message
host: centos, ip: 192.168.0.9, tag: tag3, pri: 177, syslogfacility: 22, syslogseverity: 1 msg: this is a test message' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
