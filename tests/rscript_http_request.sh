#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# add 2017-12-01 by Rainer Gerhards, released under ASL 2.0
# shellcheck disable=SC2154 # Server variables are assigned by diag.sh helpers.
# Verify http_request() returns each of ten GET response bodies unchanged.
# A private loopback echo server preserves the echo-get.php response contract
# without depending on its availability after a one-time readiness probe.
# Only the TCP input enters the request ruleset: HTTP error diagnostics must
# not recursively trigger more requests. The port file proves helper readiness;
# the line wait allows output to drain, and cmp_exact verifies all ten bodies.
. ${srcdir:=.}/diag.sh init
omhttp_start_server 0 --interface 127.0.0.1 --echo-get
# Cover assertion failures and shell exits as well as the successful path.
trap 'if [ -d "$omhttp_work_dir" ]; then omhttp_stop_server; fi' EXIT
test_error_exit_handler() {
	cat "$omhttp_server_logfile"
	omhttp_stop_server
}
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/fmhttp/.libs/fmhttp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="request")

# for debugging the test itself:
#template(name="outfmt" type="string" string="%$!%:  :%$.%:  %rawmsg%\n")
template(name="outfmt" type="string" string="%$!%\n")

ruleset(name="request") {
	set $.url = "http://127.0.0.1:'$omhttp_server_lstnport'/testbench/echo-get.php?content=" & ltrim($msg);
	set $!reply = http_request($.url);
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m10
wait_file_lines $RSYSLOG_OUT_LOG 10 200
shutdown_when_empty
wait_shutdown

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
omhttp_stop_server
exit_test

