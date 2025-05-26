#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=50000

port="$(get_free_port)"
omhttp_start_server $port --fail-every 100 --fail-with 207

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

main_queue(queue.dequeueBatchSize="2048")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

# Echo message as-is for retry
template(name="tpl_echo" type="string" string="%msg%\n")

# Echo response as-is for retry
template(name="tpl_response" type="string" string="{ \"message\": %msg%, \"response\": %$!omhttp!response% }\n")

ruleset(name="ruleset_omhttp_retry") {
    #action(type="omfile" file="'$RSYSLOG_DYNNAME/omhttp.message.log'" template="tpl_echo")
    # log the response
    action(type="omfile" file="'$RSYSLOG_DYNNAME/omhttp.response.log'" template="tpl_response")
    action(
        name="action_omhttp"
        type="omhttp"
        errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
        template="tpl_echo"

        server="localhost"
        serverport="'$port'"
        restpath="my/endpoint"
        batch="on"
        batch.maxsize="100"
        batch.format="kafkarest"

        httpretrycodes=["207","500"]
        retry="on"
        retry.ruleset="ruleset_omhttp_retry"
        retry.addmetadata="on"

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
        serverport="'$port'"
        restpath="my/endpoint"
        batch="on"
        batch.maxsize="100"
        batch.format="kafkarest"

        httpretrycodes=["207", "500"]
        retry="on"
        retry.ruleset="ruleset_omhttp_retry"
        retry.addmetadata="on"

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
omhttp_get_data $port my/endpoint kafkarest
omhttp_stop_server
seq_check
cat -n ${RSYSLOG_DYNNAME}/omhttp.response.log
omhttp_validate_metadata_response
exit_test
