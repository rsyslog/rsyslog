#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# Test batch functionality of omawslogshlc: send enough messages to trigger
# multiple batches and verify all are posted successfully.
. ${srcdir:=.}/diag.sh init
. ${srcdir:=.}/omawslogshlc-env.sh

require_plugin "omawslogshlc"

export NUMMESSAGES=50

omawslogshlc_require_env

generate_conf
add_conf '
global(processInternalMessages="on" internalmsg.severity="info")

module(load="../plugins/omawslogshlc/.libs/omawslogshlc")

local4.* action(
	type="omawslogshlc"
	region="'$AWS_CWL_REGION'"
	bearer_token="'$AWS_CWL_BEARER_TOKEN'"
	log_group="'$AWS_CWL_LOG_GROUP'"
	log_stream="'$AWS_CWL_LOG_STREAM'"
	max_batch_size="10"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
)

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

content_check "omawslogshlc: batch posted successfully"

exit_test
