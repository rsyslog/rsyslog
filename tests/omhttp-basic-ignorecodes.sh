#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
#
# Verify that omhttp treats configured HTTP status codes as ignorable. The test
# server fails requests after the first half of the message stream; success is
# that only the accepted first half remains and the ignored failures do not
# break shutdown or sequence validation.

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000

omhttp_start_server 0 --fail-with-401-or-403-after 5000
port="$omhttp_server_lstnport"

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../contrib/omhttp/.libs/omhttp")

if $msg contains "msgnum:" then
	action(
		# Payload
		name="my_http_action"
		type="omhttp"
		errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
		template="tpl"

		server="localhost"
		serverport="'$port'"
		restpath="my/endpoint"
		batch="off"
		httpignorablecodes=["401", "NA", "403"]

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
seq_check 0 4999
exit_test
