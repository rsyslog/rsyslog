#!/bin/bash
# Verify RainerScript reloadOnHUP=on parses the candidate without side effects,
# then fails closed because activation is not implemented in this step. The
# internal structured record is the deterministic oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
issue_HUP
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=activation_not_implemented'
content_check 'reload_on_total=1'
content_check 'reload_on_rejected_total=1'
content_check 'reload_legacy_hook_total=1'
assert_content_missing 'result=validated'
exit_test
