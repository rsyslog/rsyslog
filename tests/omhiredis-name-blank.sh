#!/usr/bin/env bash
# added 2025-12-04 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
# export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/omhiredis/.libs/omhiredis")
template(name="outfmt" type="string" string="%msg%")

local4.* {
        action(type="omhiredis"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="queue"
                key="myKey"
                template="outfmt")

        stop
}
'

startup

injectmsg 1 1

shutdown_when_empty
wait_shutdown

content_check "omhiredis[action-1-../contrib/omhiredis/.libs/omhiredis]:" ${RSYSLOG_DYNNAME}.started
check_not_present "omhiredis:" ${RSYSLOG_DYNNAME}.started
stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
