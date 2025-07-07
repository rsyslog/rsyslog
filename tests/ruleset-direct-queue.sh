#!/bin/bash
# check that ruleset is called synchronously when queue.type="direct" is
# specified in ruleset.
# added 2021-09-17 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export STATSFILE="$RSYSLOG_DYNNAME.stats"
add_conf '
template(name="outfmt" type="string" string="%msg%,%$.msg%\n")

ruleset(name="rs1" queue.type="direct") {
	set $.msg = "TEST";
}


if $msg contains "msgnum:" then {
	set $.msg = $msg;
	call rs1
	action(type="omfile" name="main-action" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
content_check "msgnum:00000000:,TEST"
exit_test
