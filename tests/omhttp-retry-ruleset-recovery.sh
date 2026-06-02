#!/bin/bash
# add 2026-05-31 by OpenAI Codex
# This is a regression test for issue #5693.  It verifies that an omhttp
# action using retry.ruleset can recover after a short HTTP outage instead of
# stalling behind retry-ruleset queue flow-control watermarks.  The helper
# fails requests for one second starting with the first POST, then accepts
# traffic again.  Success is proven by the normal sequence oracle over all
# generated messages after rsyslog drains and shuts down.  TEST_MAX_RUNTIME is
# intentionally bounded at 120s so the historical stall fails quickly while
# leaving enough room for CI scheduling and retry backoff jitter.
. ${srcdir:=.}/diag.sh init

export TEST_MAX_RUNTIME=${TEST_MAX_RUNTIME:-120}
NUMMESSAGES=200

omhttp_start_server 0 --fail-interval-start 0 --fail-interval-stop 1
# shellcheck disable=SC2154 # set by omhttp_start_server in diag.sh
port="$omhttp_server_lstnport"

fetch_omhttp_data() {
	omhttp_get_data "$port" my/endpoint kafkarest
}

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

template(name="tpl" type="string"
	string="{\"msgnum\":\"%msg:F,58:2%\"}")

template(name="tpl_retry" type="string" string="%msg%")

ruleset(name="rs_q_default" queue.type="LinkedList" queue.size="500") {
	action(type="omhttp"
		name="default_omhttp"
		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		template="tpl"
		batch="on"
		batch.format="kafkarest"
		batch.maxsize="10"
		retry="on"
		retry.ruleset="rs_q_retry"
		usehttps="off"
		action.resumeRetryCount="-1"
		action.resumeInterval="1"
		action.resumeIntervalMax="1"
		checkpath="ping")
}

ruleset(name="rs_q_retry" queue.type="LinkedList" queue.size="5000" queue.lightdelaymark="100" queue.fulldelaymark="150" queue.timeoutenqueue="30000") {
	action(type="omhttp"
		name="retry_omhttp"
		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		template="tpl_retry"
		batch="on"
		batch.format="kafkarest"
		batch.maxsize="10"
		usehttps="off"
		action.resumeRetryCount="-1"
		action.resumeInterval="1"
		action.resumeIntervalMax="1"
		checkpath="ping")
}

if $msg contains "msgnum:" then
	call rs_q_default
'
startup
injectmsg 0 "$NUMMESSAGES"

PRE_SEQ_CHECK_FUNC=fetch_omhttp_data wait_seq_check 0 $((NUMMESSAGES - 1)) -d
shutdown_when_empty
wait_shutdown
fetch_omhttp_data
omhttp_stop_server
seq_check 0 $((NUMMESSAGES - 1)) -d
exit_test
