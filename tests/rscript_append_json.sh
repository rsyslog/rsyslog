#!/bin/bash
# Added 2025-12-26 by 20syldev, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!arr%|%$!obj_new%|%$!chain%\n")

if $msg contains "msgnum:" then {
    # Test 1: Append string to array
    set $.ret = parse_json('\''["a", "b"]'\'', "\$!arr");
    set $!arr = append_json($!arr, "c");

    # Test 2: Append key-value to object (use new variable to test)
    set $.ret = parse_json('\''{"name": "test"}'\'', "\$!obj");
    set $!obj_new = append_json($!obj, "status", "active");

    # Test 3: Chain multiple appends
    set $.ret = parse_json('\''[]'\'', "\$!chain");
    set $!chain = append_json($!chain, "first");
    set $!chain = append_json($!chain, "second");

    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='[ "a", "b", "c" ]|{ "name": "test", "status": "active" }|[ "first", "second" ]'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
