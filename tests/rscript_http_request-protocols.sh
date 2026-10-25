#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify http_request() refuses non-HTTP URLs and redacts logged URLs.
# A file:// request for a local secret must return the failure value 0
# without the file content. A failing http:// request with userinfo and a
# query must log only scheme, host and path. Internal error messages reach
# the default ruleset, which writes them to a separate log for the check.
. ${srcdir:=.}/diag.sh init
printf 'FMHTTP-SECRET\n' > "$RSYSLOG_DYNNAME.secret"
secret_path="$(pwd)/$RSYSLOG_DYNNAME.secret"
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/fmhttp/.libs/fmhttp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="request")

template(name="outfmt" type="string" string="%$!file%,%$!refused%\n")
template(name="errfmt" type="string" string="%msg%\n")

if $msg contains "http_request" then
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.errlog" template="errfmt")

ruleset(name="request") {
	set $!file = http_request("file://'$secret_path'");
	set $!refused = http_request("http://user:pass@127.0.0.1:1/path?token=TOPSECRET#frag");
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1
wait_file_lines $RSYSLOG_OUT_LOG 1 200
shutdown_when_empty
wait_shutdown

export EXPECTED='0,0'
cmp_exact $RSYSLOG_OUT_LOG

if grep -q 'FMHTTP-SECRET' "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.errlog"; then
	echo "FAIL: file content was returned or logged"
	error_exit 1
fi
content_check "http_request failed, URL: 'file://" "$RSYSLOG_DYNNAME.errlog"
content_check "URL: 'http://127.0.0.1:1/path'" "$RSYSLOG_DYNNAME.errlog"
if grep -q -E 'TOPSECRET|user:pass|frag' "$RSYSLOG_DYNNAME.errlog"; then
	echo "FAIL: logged URL was not redacted"
	cat "$RSYSLOG_DYNNAME.errlog"
	error_exit 1
fi
exit_test
