#!/bin/bash
. ${srcdir:=.}/diag.sh init
UNEXPECTED_LOG="${RSYSLOG_DYNNAME}.unexpected.log"
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "this-needle-is-clearly-longer-than-the-message-under-test" then {
        action(type="omfile" template="outfmt" file="'$UNEXPECTED_LOG'")
}
if $syslogtag == "app" then {
        action(type="omfile" template="outfmt" file="'${RSYSLOG_OUT_LOG}'")
}
'
startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host app - - - short'
shutdown_when_empty
wait_shutdown
export EXPECTED="short"
cmp_exact
if test -e "$UNEXPECTED_LOG"; then
        test ! -s "$UNEXPECTED_LOG"
fi
exit_test
