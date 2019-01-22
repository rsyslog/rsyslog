#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

port="$(get_free_port)"
omhttp_start_server $port

error_file=$(pwd)/$RSYSLOG_DYNNAME.omhttp.error.log
rm -f $error_file

generate_conf
add_conf '
#$DebugLevel 2
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../contrib/omhttp/.libs/omhttp")

if $msg contains "msgnum:" then
	action(
		# Payload
		name="my_http_action"
		type="omhttp"
		errorfile="'$error_file'"
		template="tpl"

		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		batch="off"

		# Auth
		usehttps="off"
    )
'
startup
injectmsg  0 10000
shutdown_when_empty
wait_shutdown
omhttp_get_data $port my/endpoint
omhttp_stop_server
seq_check  0 9999
exit_test
