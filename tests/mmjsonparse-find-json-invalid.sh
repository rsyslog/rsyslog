#!/bin/bash
# Test mmjsonparse find-json mode with invalid JSON
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

# Test invalid JSON
if $msg contains "INVALID" then {
    action(type="mmjsonparse" mode="find-json")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}

# Test no JSON present
if $msg contains "NOJSON" then {
    action(type="mmjsonparse" mode="find-json")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}
'
startup
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: INVALID prefix {invalid json'
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: NOJSON just plain text without any json'
shutdown_when_empty
wait_shutdown

export EXPECTED=' INVALID prefix {invalid json parsesuccess=FAIL json={ "msg": " INVALID prefix {invalid json" }
 NOJSON just plain text without any json parsesuccess=FAIL json={ "msg": " NOJSON just plain text without any json" }'
cmp_exact
exit_test
