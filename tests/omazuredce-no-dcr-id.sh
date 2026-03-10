#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

require_plugin "omazuredce"

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omazuredce/.libs/omazuredce")

template(name="tplAzureDceNoDcrId" type="list" option.jsonftree="on") {
	property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
	property(outname="host" name="hostname" format="jsonf")
	property(outname="message" name="msg" format="jsonf")
}

local4.* action(
	type="omazuredce"
	template="tplAzureDceNoDcrId"
	client_id="dummy-client-id"
	client_secret=`echo dummy-client-secret`
	tenant_id="dummy-tenant-id"
	dce_url="https://example.com"
	table_name="Custom-Test_CL"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown

content_check "omazuredce: parameter 'dcr_id' required but not specified"

exit_test
