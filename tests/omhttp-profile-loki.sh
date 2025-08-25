#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# Test the profile="loki" configuration

#  Starting actual testbench
. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=100

# Start a mock Loki server (enable decompression as profile enables compression)
omhttp_start_server 0 --decompress

generate_conf
add_conf '
module(load="../contrib/omhttp/.libs/omhttp")

# Simplified loki payload to match test harness lokirest parser
template(name="loki_template" type="string" string="{\"msgnum\":\"%msg:F,58:2%\"}")

ruleset(name="ruleset_omhttp_loki") {
    action(
        name="action_omhttp_loki"
        type="omhttp"

        # Use the Loki profile
        profile="loki"

        template="loki_template"
        server="localhost"
        serverport="'$omhttp_server_lstnport'"

        # The profile should set these defaults:
        # - batch.format="lokirest"
        # - restpath="loki/api/v1/push"
        # - batch="on"
        # - compress="on"

        # Our mock server is plain HTTP
        usehttps="off"
    ) & stop
}

if $msg contains "msgnum:" then
    call ruleset_omhttp_loki
'
startup
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

# Verify data was sent to the Loki endpoint; parse as lokirest
omhttp_get_data $omhttp_server_lstnport loki/api/v1/push lokirest

omhttp_stop_server

# Verify all messages were sent
seq_check 0 $(( NUMMESSAGES - 1 ))
exit_test
