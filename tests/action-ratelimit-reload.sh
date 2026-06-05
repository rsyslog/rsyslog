#!/bin/bash
# Verify reload behavior for output-scoped rate limit policies.
# A HUP with an invalid scope change must keep the previous policy active; a
# later valid HUP lowers burst to zero, proving output policy reload applies
# without changing the action reference.
. ${srcdir:=.}/diag.sh init

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.output-reload.yaml"
write_policy() {
	cat > "$POLICY_FILE" << YAMLEOF
$1
YAMLEOF
}

write_policy 'scope: output
mode: drop
interval: 60
burst: 100'

generate_conf
add_conf '
ratelimit(name="out_reload" policy="'$POLICY_FILE'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
                                 template="outfmt"
                                 action.ratelimit.name="out_reload")
'
startup
injectmsg 0 5
wait_queueempty
content_count_check "000000" 5 "$RSYSLOG_OUT_LOG"

: > "$RSYSLOG_OUT_LOG"
write_policy 'scope: input
mode: drop
interval: 60
burst: 0'
issue_HUP
./msleep 1000
injectmsg 5 5
wait_queueempty
content_count_check "000000" 5 "$RSYSLOG_OUT_LOG"

: > "$RSYSLOG_OUT_LOG"
write_policy 'scope: output
mode: drop
interval: 60
burst: 0'
issue_HUP
./msleep 1000
injectmsg 10 5
wait_queueempty
content_count_check "000000" 0 "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
exit_test
