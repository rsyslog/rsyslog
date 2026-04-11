#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/omazuredce-env.sh

require_plugin "omazuredce"

export NUMMESSAGES=10

omazuredce_require_env

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omazuredce/.libs/omazuredce")

template(name="tplAzureDceCompression" type="list" option.jsonftree="on") {
	property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="message" name="msg" format="jsonf")
}

	local4.* action(
		type="omazuredce"
		template="tplAzureDceCompression"
	client_id="'$AZURE_DCE_CLIENT_ID'"
	client_secret=`echo $AZURE_DCE_CLIENT_SECRET`
	tenant_id="'$AZURE_DCE_TENANT_ID'"
		dce_url="'$AZURE_DCE_URL'"
		dcr_id="'$AZURE_DCE_DCR_ID'"
		table_name="'$AZURE_DCE_TABLE_NAME'"
		compression="default"
		max_batch_bytes="1048576"
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

content_check "compression=default"

exit_test
