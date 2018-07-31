#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="customparser")
parser(name="custom.rfc3164" type="pmrfc3164" remove.msgFirstSpace="on")
template(name="outfmt" type="string" string="-%msg%-\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	:syslogtag, contains, "tag" action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:1\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag:  msgnum:2\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag:msgnum:3\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag4:\""
shutdown_when_empty
wait_shutdown
echo '-msgnum:1-
- msgnum:2-
-msgnum:3-
--' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
