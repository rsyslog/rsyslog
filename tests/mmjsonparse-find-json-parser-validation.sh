#!/bin/bash
# Test mmjsonparse find-json mode with improved JSON parser validation
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")

template(name="outfmt" type="string" string="%msg% parsesuccess=%parsesuccess% json=%$!%\n")

# Test various JSON edge cases that manual brace counting might miss
if $msg contains "TEST" then {
    action(type="mmjsonparse" mode="find-json")
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
    stop
}
'
startup
# Valid JSON with nested objects and arrays
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TEST1 prefix {"key": {"nested": ["a", "b"]}, "num": 123}'
# JSON with escaped quotes in strings
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TEST2 prefix {"message": "He said \"hello\" to me"}'
# JSON with braces in string values (should not confuse parser)
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TEST3 prefix {"code": "if (x > 0) { return true; }"}'
# Invalid JSON that looks like it might work with brace counting
injectmsg_literal '<167>Jan 16 16:57:54 host.example.net TAG: TEST4 prefix {"invalid": json}'
shutdown_when_empty
wait_shutdown

export EXPECTED=' TEST1 prefix {"key": {"nested": ["a", "b"]}, "num": 123} parsesuccess=OK json={ "key": { "nested": [ "a", "b" ] }, "num": 123 }
 TEST2 prefix {"message": "He said \"hello\" to me"} parsesuccess=OK json={ "message": "He said \"hello\" to me" }
 TEST3 prefix {"code": "if (x > 0) { return true; }"} parsesuccess=OK json={ "code": "if (x > 0) { return true; }" }
 TEST4 prefix {"invalid": json} parsesuccess=FAIL json={ "msg": " TEST4 prefix {\"invalid\": json}" }'
cmp_exact
exit_test