#!/bin/bash
# add 2017-03-06 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset")
template(name="test" type="string" string="tag: %syslogtag%, pri: %pri%, syslogfacility: %syslogfacility%, syslogseverity: %syslogseverity% msg: %msg%\n")
ruleset(name="ruleset" parser="rsyslog.rfc5424") {
	action(type="omfile" file="rsyslog2.out.log" template="test")
}
action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m100
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
echo 'one message could not be processed by any parser' | cmp rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
