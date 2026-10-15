#!/bin/bash
# Verify default reloadOnHUP=off remains observational only and does not gate
# legacy HUP hooks. The first injected record is written before the output is
# renamed; after HUP, the second must appear in the new path, proving omfile
# reopened it while the manager records mode=off without activation.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(processInternalMessages="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg 0 1
wait_queueempty
mv "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG"
issue_HUP
injectmsg 1 1
wait_queueempty
shutdown_when_empty
wait_shutdown

content_check 'msgnum:00000000' "$RSYSLOG2_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'shadow_reload event=request result=ignored mode=off' "$RSYSLOG_OUT_LOG"
content_check 'rejected_reason=mode_off' "$RSYSLOG_OUT_LOG"

exit_test
