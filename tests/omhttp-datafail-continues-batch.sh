#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
#
# Regression test for rsyslog/rsyslog#4348 in omhttp batch mode. A permanent
# HTTP 400 rejects the whole HTTP request payload; for a multi-message batch,
# that means all events in that batch are failed together. The mock server
# rejects the batch containing message 00000002. The neighboring message in
# that failed HTTP payload can vary with transaction boundaries, so the oracle
# checks the stable behavior: message 2 is absent, later batches are still sent,
# and messages known to be outside the rejected payload are not duplicated.

# shellcheck disable=SC2034,SC2154

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10

omhttp_start_server 0 --fail-msgnum-400 00000002

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

main_queue(queue.dequeueBatchSize="10")

if $msg contains "msgnum:" then
	action(
		name="action_omhttp"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="my/endpoint"
		batch="on"
		batch.format="jsonarray"
		batch.maxsize="2"

		usehttps="off"
	)
'

startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint jsonarray
omhttp_stop_server
content_count_check "00000000" 1
check_not_present "00000002"
content_count_check "00000004" 1
content_count_check "00000005" 1
content_count_check "00000006" 1
content_count_check "00000007" 1
content_count_check "00000008" 1
content_count_check "00000009" 1
exit_test
