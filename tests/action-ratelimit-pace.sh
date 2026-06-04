#!/bin/bash
# Verify generic action-level output rate limiting in pace mode.
# The linked-list action queue is required so pacing sleeps only the action
# worker. Four messages with burst two must all be delivered, and elapsed time
# must show that the second window was reached instead of dropping excess
# messages.
. ${srcdir:=.}/diag.sh init

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.output-pace.yaml"
cat > "$POLICY_FILE" << 'YAMLEOF'
scope: output
mode: pace
interval: 1
burst: 2
YAMLEOF

generate_conf
add_conf '
ratelimit(name="out_pace" policy="'$POLICY_FILE'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
                                 template="outfmt"
                                 queue.type="linkedlist"
                                 action.ratelimit.name="out_pace")
'
startup
start_time=$(date +%s)
injectmsg 0 4
shutdown_when_empty
wait_shutdown
end_time=$(date +%s)

content_count_check "000000" 4 "$RSYSLOG_OUT_LOG"
seq_check 0 3
elapsed=$((end_time - start_time))
if [ "$elapsed" -lt 1 ]; then
	echo "FAIL: pace mode delivered all messages without observable pacing (elapsed ${elapsed}s)"
	error_exit 1
fi
exit_test
