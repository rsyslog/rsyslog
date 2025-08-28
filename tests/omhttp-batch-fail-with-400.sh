#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000

omhttp_start_server 0 --fail-with-400-after 1000

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

main_queue(queue.dequeueBatchSize="500")

template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

# Wrap message as a single batch for retry
template(name="tpl_retry" type="string" string="[%msg%]")


ruleset(name="ruleset_omhttp") {
    action(
        name="action_omhttp"
        type="omhttp"
        errorfile="'$RSYSLOG_DYNNAME/omhttp.error.log'"
        template="tpl"

        server="localhost"
        serverport="'$omhttp_server_lstnport'"
        restpath="my/endpoint"
        batch="off"

        retry="on"
        
        # Configure retry codes - only retry 5xx server errors, not 4xx client errors
        # This fixes the bug where HTTP 400 errors were incorrectly retried
        httpretrycodes=["500", "502", "503", "504"]

        # Auth
        usehttps="off"
    ) & stop
}

if $msg contains "msgnum:" then
    call ruleset_omhttp
'
startup
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown
omhttp_get_data $omhttp_server_lstnport my/endpoint
omhttp_stop_server
# Test expects only the first 1000 messages to be processed successfully
# Messages after 1000 get HTTP 400 errors and should NOT be retried
seq_check $SEQ_CHECK_OPTIONS 0 999
exit_test
