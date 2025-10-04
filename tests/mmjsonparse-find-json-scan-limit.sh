#!/bin/bash
# Test mmjsonparse find-json mode with max_scan_bytes limit
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

# Test with very small max_scan_bytes to force truncation
if $msg contains "SCAN_LIMIT" then {
    action(type="mmjsonparse" mode="find-json" max_scan_bytes="10")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}

# Test with sufficient max_scan_bytes
if $msg contains "SCAN_OK" then {
    action(type="mmjsonparse" mode="find-json" max_scan_bytes="100")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}
'
startup
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: SCAN_LIMIT this is a long prefix before {"test":"value"}'
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: SCAN_OK short {"test":"value"}'
shutdown_when_empty
wait_shutdown

export EXPECTED=' SCAN_LIMIT this is a long prefix before {"test":"value"} parsesuccess=FAIL json={ "msg": " SCAN_LIMIT this is a long prefix before {\"test\":\"value\"}" }
 SCAN_OK short {"test":"value"} parsesuccess=OK json={ "test": "value" }'
cmp_exact
exit_test