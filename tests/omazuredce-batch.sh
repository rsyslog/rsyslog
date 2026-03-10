#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/omazuredce-env.sh

require_plugin "omazuredce"
omazuredce_require_env

export NUMMESSAGES=200

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omazuredce/.libs/omazuredce")

template(name="tplAzureDceBatch" type="list" option.jsonftree="on") {
	property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="message" name="msg" format="jsonf")
}

local4.* action(
	type="omazuredce"
	template="tplAzureDceBatch"
	client_id="'$AZURE_DCE_CLIENT_ID'"
	client_secret=`echo $AZURE_DCE_CLIENT_SECRET`
	tenant_id="'$AZURE_DCE_TENANT_ID'"
	dce_url="'$AZURE_DCE_URL'"
	dcr_id="'$AZURE_DCE_DCR_ID'"
	table_name="'$AZURE_DCE_TABLE_NAME'"
	max_batch_bytes="4096"
	flush_timeout_ms="0"
	action.resumeInterval="1"
	action.resumeRetryCount="1"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

content_check "omazuredce: posted batch records="

post_count=$(grep -c -F "omazuredce: posted batch records=" < "$RSYSLOG_OUT_LOG")
if [ "${post_count:=0}" -lt 2 ]; then
	echo "FAIL: expected multiple omazuredce batch posts, got $post_count"
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
