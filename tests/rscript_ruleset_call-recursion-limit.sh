#!/bin/bash
# This covers runaway synchronous ruleset recursion. The test deliberately
# creates a self-calling ruleset and proves that rsyslog logs the recursion
# guard diagnostic through its configured omfile destination after synchronized
# shutdown instead of crashing or relying on stdout/stderr.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
template(name="counter" type="string" string="%$.counter%\n")

ruleset(name="recurse") {
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="counter")
    set $.counter = $.counter + 1;
    call recurse
}

if $msg contains "msgnum:" then {
    set $.counter = 1;
    call recurse
}

syslog.* {
    action(type="omfile" template="outfmt" file=`echo $RSYSLOG2_OUT_LOG`)
}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check "1024" "$RSYSLOG_OUT_LOG"
custom_assert_content_missing "1025" "$RSYSLOG_OUT_LOG"
content_check "ruleset call nesting limit of 1024 reached" "$RSYSLOG2_OUT_LOG"
custom_assert_content_missing "Segmentation fault" "$RSYSLOG2_OUT_LOG"

exit_test
