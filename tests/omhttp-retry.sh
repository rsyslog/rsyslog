#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000

omhttp_start_server 0 --fail-every 1000

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

main_queue(queue.dequeueBatchSize="2048")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

if $msg contains "msgnum:" then
	action(
		# Payload
		action.resumeRetryCount="-1"
		name="my_http_action"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="my/endpoint"
		checkpath="ping"
		batch="off"

		# Auth
		usehttps="off"
  )
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint
omhttp_stop_server
seq_check
exit_test
