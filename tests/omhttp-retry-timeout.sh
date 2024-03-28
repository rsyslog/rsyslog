#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000

port="$(get_free_port)"
omhttp_start_server $port --fail-every 1000 --fail-with-delay-secs 2

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
		action.resumeInterval="1"
		name="my_http_action"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		restpathtimeout="1000"
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
omhttp_get_data $port my/endpoint
omhttp_stop_server
seq_check
exit_test
