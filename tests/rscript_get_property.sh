#!/bin/bash
# released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$!%\n")
template(name="outlocal" type="string" string="%$.%\n")

if $msg contains "msgnum" then {
  set $.ret = parse_json("{\"offsets\": [ { \"a\": 9, \"b\": 0, \"c\": \"boo\", \"d\": null },\
                                         { \"a\": 9, \"b\": 3, \"c\": null, \"d\": null } ],\
                                         \"booltest\": true,\
                                         \"int64\": 1234567890,\
                                         \"nulltest\": null,\
                                         \"double\": 12345.67890,\
                                         \"foo\": 3, \"bar\": 28 }", "\$!parsed");

	if $.ret == 0 then {
		# set up some locals
		set $!foo!bar = 3;
		set $.index = "1";
		set $.test = "a";

		# evaluate
		set $.res1 = get_property($!parsed!offsets, $.index);
		set $.res2 = get_property($!parsed!offsets[1], $.test);
		reset $.test = "bar";
		set $.res3 = get_property($!foo, $.test);
		reset $.index = 5;
		set $.res4 = get_property($!parsed!offsets, $.index);
		set $.key = "test";
		set $.res5 = get_property($., $.key);
		reset $.key = "foo";
		set $.res6 = get_property($!, $.key);
		set $.res7 = get_property($!foo, "bar");
		reset $.key = "ar";
		set $.res8 = get_property($!foo, "b" & $.key);
		set $.res9 = get_property($!foo!bar, "");
		reset $.key = "";
		set $.res10 = get_property($!foo!bar, $.key);
		set $.res11 = get_property($!parsed!booltest, "");
		reset $.key = "int64";
		set $.res12 = get_property($!parsed, $.key);
		reset $.key = "nulltest";
		set $.res13 = get_property($!parsed, $.key);
		reset $.key = "double";
		set $.res14 = get_property($!parsed, $.key);
		set $.res15 = get_property($msg, "");
		set $.res16 = get_property("string literal", "");

		# output result
		unset $.key;
		action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
		action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outlocal")
	}
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

EXPECTED='{ "parsed": { "offsets": [ { "a": 9, "b": 0, "c": "boo", "d": null }, { "a": 9, "b": 3, "c": null, "d": null } ], "booltest": true, "int64": 1234567890, "nulltest": null, "double": 12345.67890, "foo": 3, "bar": 28 }, "foo": { "bar": 3 } }
{ "ret": 0, "index": 5, "test": "bar", "res1": { "a": 9, "b": 3, "c": null, "d": null }, "res2": 9, "res3": 3, "res4": "", "res5": "bar", "res6": { "bar": 3 }, "res7": 3, "res8": 3, "res9": 3, "res10": 3, "res11": 1, "res12": 1234567890, "res13": "", "res14": 12345.678900000001, "res15": " msgnum:00000000:", "res16": "" }'
cmp_exact

exit_test
