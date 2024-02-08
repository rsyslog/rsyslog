#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"

omhttp_start_server 0

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")
template(name="dynrestpath" type="string" string="my/endpoint")

module(load="../contrib/omhttp/.libs/omhttp")

if $msg contains "msgnum:" then
	action(
		# Payload
		name="my_http_action"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$omhttp_server_lstnport'"
		dynrestpath = "on"
		restpath="dynrestpath"

		batch="on"
		batch.format="jsonarray"
		batch.maxsize="1000"

		# Auth
		usehttps="off"
    )
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint jsonarray
omhttp_stop_server
seq_check
exit_test
