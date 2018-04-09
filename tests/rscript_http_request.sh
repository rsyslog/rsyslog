#!/bin/bash
# add 2017-12-01 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh check-url-access http://testbench.rsyslog.com/testbench/echo-get.php
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/fmhttp/.libs/fmhttp")
input(type="imtcp" port="13514")

# for debugging the test itself:
#template(name="outfmt" type="string" string="%$!%:  :%$.%:  %rawmsg%\n")
template(name="outfmt" type="string" string="%$!%\n")

if $msg contains "msgnum:" then {
	set $.url = "http://testbench.rsyslog.com/testbench/echo-get.php?content=" & ltrim($msg);
	set $!reply = http_request($.url);
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo '{ "reply": "msgnum:00000000:" }
{ "reply": "msgnum:00000001:" }
{ "reply": "msgnum:00000002:" }
{ "reply": "msgnum:00000003:" }
{ "reply": "msgnum:00000004:" }
{ "reply": "msgnum:00000005:" }
{ "reply": "msgnum:00000006:" }
{ "reply": "msgnum:00000007:" }
{ "reply": "msgnum:00000008:" }
{ "reply": "msgnum:00000009:" }' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid function output detected, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;
. $srcdir/diag.sh exit

