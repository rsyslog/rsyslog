#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
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
                mode="publish"
                key="myChannel"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'
(redis_command "SUBSCRIBE myChannel" > $RSYSLOG_OUT_LOG) &

startup

# Inject 3 messages
injectmsg 1 3

shutdown_when_empty
wait_shutdown

# The SUBSCRIBE command gets the result of the subscribing and the 3 subsequent messages published by omhiredis
export EXPECTED="/usr/bin/redis-cli
subscribe
myChannel
1
message
myChannel
 msgnum:00000001:
message
myChannel
 msgnum:00000002:
message
myChannel
 msgnum:00000003:"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
