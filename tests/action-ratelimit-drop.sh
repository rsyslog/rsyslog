#!/bin/bash
# Verify generic action-level output rate limiting in drop mode.
# The policy allows five messages in a long window; the oracle is that exactly
# the first five injected messages reach omfile and later matching messages are
# intentionally discarded before the output module runs.
. ${srcdir:=.}/diag.sh init

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.output-drop.yaml"
cat > "$POLICY_FILE" << 'YAMLEOF'
scope: output
mode: drop
interval: 60
burst: 5
YAMLEOF

generate_conf
add_conf '
ratelimit(name="out_drop" policy="'$POLICY_FILE'")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
                                 template="outfmt"
                                 action.ratelimit.name="out_drop")
'
startup
injectmsg 0 20
shutdown_when_empty
wait_shutdown

content_count_check "000000" 5 "$RSYSLOG_OUT_LOG"
seq_check 0 4
exit_test
