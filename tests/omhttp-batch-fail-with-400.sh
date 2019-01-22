#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

port="$(get_free_port)"
omhttp_start_server $port --fail-with-400-after 1000

error_file=$(pwd)/$RSYSLOG_DYNNAME.omhttp.error.log
rm -f $error_file

generate_conf
add_conf '
#$DebugLevel 2
module(load="../contrib/omhttp/.libs/omhttp")

main_queue(queue.dequeueBatchSize="2048")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

# Wrap message as a single batch for retry
template(name="tpl_retry" type="string" string="[%msg%]")


ruleset(name="ruleset_omhttp") {
    action(
        name="action_omhttp"
        type="omhttp"
        errorfile="'$error_file'"
        template="tpl"

        server="localhost"
        serverport="'$port'"
        restpath="my/endpoint"
        batch="off"

        retry="on"

        # Auth
        usehttps="off"
    ) & stop
}

if $msg contains "msgnum:" then
    call ruleset_omhttp
'
startup
injectmsg  0 10000
shutdown_when_empty
wait_shutdown
omhttp_get_data $port my/endpoint
omhttp_stop_server
seq_check  0 999
exit_test
