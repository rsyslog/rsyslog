#!/bin/bash
# added 2024-06-01 by Codex; Released under ASL 2.0
## checks endswith operator
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="string" string="%programname% %msg%\n")
if $programname endswith ["_foo", "-bar", ".baz"] then {
	action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
}
'

startup
injectmsg_literal '<165>Mar  1 00:00:00 host prog_foo: msgnum:00000000:'
injectmsg_literal '<165>Mar  1 00:00:00 host test-bar: msgnum:00000001:'
injectmsg_literal '<165>Mar  1 00:00:00 host example.baz: msgnum:00000002:'
injectmsg_literal '<165>Mar  1 00:00:00 host nomatch: msgnum:00000003:'
shutdown_when_empty
wait_shutdown
content_check "prog_foo msgnum:00000000:" "$RSYSLOG_OUT_LOG"
content_check "test-bar msgnum:00000001:" "$RSYSLOG_OUT_LOG"
content_check "example.baz msgnum:00000002:" "$RSYSLOG_OUT_LOG"
check_not_present "nomatch" "$RSYSLOG_OUT_LOG"
exit_test

