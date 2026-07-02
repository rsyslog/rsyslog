#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# Test that omawslogshlc rejects config when required parameter 'region' is missing.
. ${srcdir:=.}/diag.sh init

require_plugin "omawslogshlc"

generate_conf
add_conf '
module(load="../plugins/omawslogshlc/.libs/omawslogshlc")

local4.* action(
	type="omawslogshlc"
	bearer_token="fake-token"
	log_group="test-group"
	log_stream="test-stream"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown

content_check "parameter 'region' is required"

exit_test
