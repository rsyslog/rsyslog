#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Added 2017-12-09 by Rainer Gerhards, released under ASL 2.0
# Complete JSON values, including null and un-delimited scalars, must parse.
# Exact output verifies return/script status, stored null, scalar replacement,
# and unchanged destinations after incomplete or trailing-garbage input.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$!%\n")

local4.* {
	set $.ret = parse_json("{ \"c1\":\"data\" }", "\$!parsed");
	set $!null_result = parse_json("null", "\$!null_value");
	set $!null_error = script_error();
	set $!replaced = "old";
	set $!replace_result = parse_json("null ", "\$!replaced");
	set $!replace_error = script_error();
	set $!number_result = parse_json("42", "\$!number");
	set $!boolean_result = parse_json("true", "\$!boolean");
	set $!guard = "unchanged";
	set $!incomplete_result = parse_json("nul", "\$!guard");
	set $!incomplete_error = script_error();
	set $!trailing_result = parse_json("null garbage", "\$!guard");
	set $!trailing_error = script_error();
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "parsed": { "c1": "data" }, "null_value": null, "null_result": 0, "null_error": 0, "replaced": null, "replace_result": 0, "replace_error": 0, "number": 42, "number_result": 0, "boolean": true, "boolean_result": 0, "guard": "unchanged", "incomplete_result": 1, "incomplete_error": 1, "trailing_result": 1, "trailing_error": 1 }'
cmp_exact $RSYSLOG_OUT_LOG

exit_test
