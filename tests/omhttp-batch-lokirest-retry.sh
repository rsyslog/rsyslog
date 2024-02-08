#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=50000

omhttp_start_server 0 --fail-every 100

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

main_queue(queue.dequeueBatchSize="2048")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

# Echo message as-is for retry
template(name="tpl_echo" type="string" string="%msg%")

ruleset(name="ruleset_omhttp_retry") {
    action(
        name="action_omhttp"
        type="omhttp"
        errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
        template="tpl_echo"

        server="localhost"
        serverport="'$omhttp_server_lstnport'"
        restpath="my/endpoint"
        batch="on"
        batch.maxsize="100"
        batch.format="lokirest"

        retry="on"
        retry.ruleset="ruleset_omhttp_retry"

        # Auth
        usehttps="off"
    ) & stop
}

ruleset(name="ruleset_omhttp") {
    action(
        name="action_omhttp"
        type="omhttp"
        errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
        template="tpl"

        server="localhost"
        serverport="'$omhttp_server_lstnport'"
        restpath="my/endpoint"
        batch="on"
        batch.maxsize="100"
        batch.format="lokirest"

        retry="on"
        retry.ruleset="ruleset_omhttp_retry"

        # Auth
        usehttps="off"
    ) & stop
}

if $msg contains "msgnum:" then
    call ruleset_omhttp
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint lokirest
omhttp_stop_server
seq_check
exit_test
