#!/bin/bash
# add 2017-12-01 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
rsyslog_testbench_test_url_access http://testbench.rsyslog.com/testbench/echo-get.php
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/fmhttp/.libs/fmhttp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# for debugging the test itself:
#template(name="outfmt" type="string" string="%$!%:  :%$.%:  %rawmsg%\n")
template(name="outfmt" type="string" string="%$!%\n")

if $msg contains "msgnum:" then {
	set $.url = "http://testbench.rsyslog.com/testbench/echo-get.php?content=" & ltrim($msg);
	set $!reply = http_request($.url);
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m10
wait_file_lines $RSYSLOG_OUT_LOG 10 200
shutdown_when_empty
wait_shutdown

# check for things that look like http failures
if grep -q "DOCTYPE" "$RSYSLOG_OUT_LOG" ; then
	printf 'SKIP: looks like we have problems with the upstream http server:\n'
	cat -n "$RSYSLOG_OUT_LOG"
	printf '\n'
	error_exit 177
fi

export EXPECTED='{ "reply": "msgnum:00000000:" }
{ "reply": "msgnum:00000001:" }
{ "reply": "msgnum:00000002:" }
{ "reply": "msgnum:00000003:" }
{ "reply": "msgnum:00000004:" }
{ "reply": "msgnum:00000005:" }
{ "reply": "msgnum:00000006:" }
{ "reply": "msgnum:00000007:" }
{ "reply": "msgnum:00000008:" }
{ "reply": "msgnum:00000009:" }'
cmp_exact  $RSYSLOG_OUT_LOG
exit_test

