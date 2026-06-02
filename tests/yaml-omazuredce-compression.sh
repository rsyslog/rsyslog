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

include(file="'${RSYSLOG_DYNNAME}'.yaml")
call main

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

cp -f "$srcdir/testsuites/yaml-omazuredce-compression.yaml" "${RSYSLOG_DYNNAME}.yaml"
sed -i \
	-e "s|@AZURE_DCE_CLIENT_ID@|$AZURE_DCE_CLIENT_ID|g" \
	-e "s|@AZURE_DCE_CLIENT_SECRET@|$AZURE_DCE_CLIENT_SECRET|g" \
	-e "s|@AZURE_DCE_TENANT_ID@|$AZURE_DCE_TENANT_ID|g" \
	-e "s|@AZURE_DCE_URL@|$AZURE_DCE_URL|g" \
	-e "s|@AZURE_DCE_DCR_ID@|$AZURE_DCE_DCR_ID|g" \
	-e "s|@AZURE_DCE_TABLE_NAME@|$AZURE_DCE_TABLE_NAME|g" \
	"${RSYSLOG_DYNNAME}.yaml"

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

content_check "compression=default"

exit_test
