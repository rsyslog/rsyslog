#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify RainerScript reloadOnHUP=on rejects the request as unsupported while
# legacy HUP processing remains available. The internal structured record is
# the deterministic oracle; Release B must not report validation or activation.
# Rename the populated output before HUP and require the next record at the
# original path, proving the unconditional omfile reopen hook really ran.
# Shutdown debug output must still be enabled from normal main-thread context.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on"
       debug.onShutdown="on" debug.logFile="'$RSYSLOG_DYNNAME'.debug.log")
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
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=unsupported_release_b'
content_check 'reload_on_total=1'
content_check 'reload_on_rejected_total=1'
content_check 'reload_legacy_hook_total=1'
assert_content_missing 'result=validated'
content_check 'termination requested by signal' "$RSYSLOG_DYNNAME.debug.log"
exit_test
