#!/bin/bash
# Test mmjsonparse find-json mode with trailing data and allow_trailing parameter
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

# Test allow_trailing=on (default)
if $msg contains "TRAILING_ON" then {
    action(type="mmjsonparse" mode="find-json" allow_trailing="on")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}

# Test allow_trailing=off
if $msg contains "TRAILING_OFF" then {
    action(type="mmjsonparse" mode="find-json" allow_trailing="off")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}
'
startup
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TRAILING_ON {"test":1} garbage after'
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TRAILING_OFF {"test":2} garbage after'
shutdown_when_empty
wait_shutdown

export EXPECTED=' TRAILING_ON {"test":1} garbage after parsesuccess=OK json={ "test": 1 }
 TRAILING_OFF {"test":2} garbage after parsesuccess=FAIL json={ "msg": " TRAILING_OFF {\"test\":2} garbage after" }'
cmp_exact
exit_test