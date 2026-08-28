#!/bin/bash
# Verify that RainerScript JSON-variable reads preserve scalar, native JSON,
# missing/null, root, and NUL semantics while values flow through set. Exact
# serialized output after synchronized shutdown is the oracle: it detects a
# type/value change as well as the legacy set-side NUL removal and read-side
# first-NUL truncation rules.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%$!out%\n")

if $msg contains "msgnum:" then {
	# es_str2cstr(..., NULL) historically removes all embedded NUL bytes.
	set $!out!set_nul = b64_decode("YWIAY2QAZWY=");
	set $.ret = parse_json("{\"flat.key\":\"flat\",\"nested\":{\"value\":\"nested\"},\"string\":\"text\",\"empty\":\"\",\"null\":null,\"integer\":42,\"boolean\":true,\"double\":1.5,\"array\":[\"one\",2],\"object\":{\"child\":\"value\"},\"nul\":\"ab\\u0000cd\"}", "\$!src");
	set $.ret = parse_json("{\"string\":\"local\",\"integer\":7,\"boolean\":false}", "\$.src");
	set $.ret = parse_json("{\"string\":\"global\",\"integer\":9,\"boolean\":true}", "\$/src");

	set $!out!flat = $!src!flat.key;
	set $!out!nested = $!src!nested!value;
	set $!out!string = $!src!string;
	set $!out!empty = $!src!empty;
	set $!out!missing = $!src!missing;
	set $!out!null = $!src!null;
	set $!out!integer = $!src!integer;
	set $!out!boolean = $!src!boolean;
	set $!out!double = $!src!double;
	set $!out!array = $!src!array;
	set $!out!object = $!src!object;
	set $!out!nul_read = $!src!nul;
	set $!out!whole = $!src;
	set $!out!local_string = $.src!string;
	set $!out!local_integer = $.src!integer;
	set $!out!local_boolean = $.src!boolean;
	set $!out!local_whole = $.src;
	set $!out!global_string = $/src!string;
	set $!out!global_integer = $/src!integer;
	set $!out!global_boolean = $/src!boolean;
	set $!out!global_whole = $/src;
	action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "set_nul": "abcdef", "flat": "flat", "nested": "nested", "string": "text", "empty": "", "missing": "", "null": "", "integer": 42, "boolean": true, "double": 1.5, "array": [ "one", 2 ], "object": { "child": "value" }, "nul_read": "ab", "whole": { "flat.key": "flat", "nested": { "value": "nested" }, "string": "text", "empty": "", "null": null, "integer": 42, "boolean": true, "double": 1.5, "array": [ "one", 2 ], "object": { "child": "value" }, "nul": "ab" }, "local_string": "local", "local_integer": 7, "local_boolean": false, "local_whole": { "string": "local", "integer": 7, "boolean": false }, "global_string": "global", "global_integer": 9, "global_boolean": true, "global_whole": { "string": "global", "integer": 9, "boolean": true } }'
cmp_exact
exit_test
