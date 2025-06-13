#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=25000

omhttp_start_server 0 --fail-interval-start 0 --fail-interval-stop 30
#omhttp_start_server 0

generate_conf
add_conf '
template(name="tpl" type="string"
	string="{\"msgnum\":\"%msg:F,58:2%\"}")
template(name="tpl_retry" type="string" string="%msg%")

module(load="../contrib/omhttp/.libs/omhttp")

# Enable impstats to see the stats
module(load="../plugins/impstats/.libs/impstats"
	log.file="./'$RSYSLOG_DYNNAME'.stats.log"
	log.syslog="off"
	format="json"
	resetCounters="off"
	interval="10"
)

if $msg contains "msgnum:" then
	call rs_q_default

ruleset(name="rs_q_default" queue.type="LinkedList" queue.size="1500") {
	action(
		name="my_http_action_default"
		type="omhttp"
		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="my/endpoint"

		batch="on"
		batch.format="kafkarest"
		batch.maxsize="10"
		batch.maxbytes="1000"

		template="tpl"

		retry="on"
		retry.ruleset="rs_q_retry"

		checkpath="ping"
		restpathtimeout="1000"
		action.resumeInterval="1"
		action.resumeIntervalMax="30"
	)
	& stop
}


ruleset(name="rs_q_retry" queue.type="LinkedList" queue.size="200" queue.lightdelaymark="100" queue.fulldelaymark="150") {
	action(
		name="my_http_action_retry"
		type="omhttp"
		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="my/endpoint"

		batch="on"
		batch.format="kafkarest"
		batch.maxsize="10"
		batch.maxbytes="1000"

		template="tpl_retry"

		retry="on"
		retry.ruleset="rs_q_retry"

		checkpath="ping"
		restpathtimeout="1000"
		action.resumeInterval="1"
		action.resumeIntervalMax="30"
	)
	& stop
}
'

startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint kafkarest
omhttp_stop_server
seq_check
exit_test
