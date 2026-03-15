#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/omazuredce-env.sh

require_plugin "omazuredce"
omazuredce_require_env

export NUMMESSAGES=5

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omazuredce/.libs/omazuredce")

template(name="tplAzureDceTimer" type="list" option.jsonftree="on") {
	property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="message" name="msg" format="jsonf")
}

local4.* action(
	type="omazuredce"
	template="tplAzureDceTimer"
	client_id="'$AZURE_DCE_CLIENT_ID'"
	client_secret=`echo $AZURE_DCE_CLIENT_SECRET`
	tenant_id="'$AZURE_DCE_TENANT_ID'"
	dce_url="'$AZURE_DCE_URL'"
	dcr_id="'$AZURE_DCE_DCR_ID'"
	table_name="'$AZURE_DCE_TABLE_NAME'"
	max_batch_bytes="1048576"
	flush_timeout_ms="200"
	action.resumeInterval="1"
	action.resumeRetryCount="1"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg 0 $NUMMESSAGES

# Verify the timer thread flushes without needing shutdown-triggered cleanup.
content_check_with_count "omazuredce: posted batch records=5" 1 5

shutdown_when_empty
wait_shutdown

post_count=$(grep -c -F "omazuredce: posted batch records=" < "$RSYSLOG_OUT_LOG")
if [ "${post_count:=0}" -ne 1 ]; then
	echo "FAIL: expected exactly one timer-driven omazuredce batch post, got $post_count"
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
