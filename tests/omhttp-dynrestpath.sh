#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=100

omhttp_start_server 0

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="my/path/%msg%")

module(load="../contrib/omhttp/.libs/omhttp")

if $msg contains "msgnum:" then
	action(
		# Payload
		name="my_http_action"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"
		dynrestpath="on"

		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		restpath="tpl"
		batch="off"

		# Auth
		usehttps="off"
    )
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport "my/path/"
omhttp_stop_server
seq_check
exit_test
