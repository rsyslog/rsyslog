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
                mode="stream"
                key="outStream"
                stream.capacityLimit="128"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

startup


# Inject 1000 messages
injectmsg 1 1000

shutdown_when_empty
wait_shutdown

count=$(redis_command "XLEN outStream" | grep -o "[0-9]*")

# Should be less than 1000 (close to 128, but not 128)
# 500 still means that stream length was capped
if [ ! "${count}" -le 500 ]; then
    echo "error: stream has too much entries -> $count"
    error_exit 1
fi

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
