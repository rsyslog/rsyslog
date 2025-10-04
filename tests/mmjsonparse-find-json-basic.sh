#!/bin/bash
# Test mmjsonparse find-json mode basic functionality
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

# Legacy cookie mode (default) - should NOT parse text with JSON
if $msg contains "LEGACY" then {
    action(type="mmjsonparse")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}

# Find-JSON mode - should parse text with embedded JSON
if $msg contains "FINDJSON" then {
    action(type="mmjsonparse" mode="find-json")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}
'
startup
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: LEGACY prefix {"field":"value"}'
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: FINDJSON prefix {"field":"value"}'
shutdown_when_empty
wait_shutdown

export EXPECTED=' LEGACY prefix {"field":"value"} parsesuccess=FAIL json={ "msg": "LEGACY prefix {\"field\":\"value\"}" }
 FINDJSON prefix {"field":"value"} parsesuccess=OK json={ "field": "value" }'
cmp_exact
exit_test
