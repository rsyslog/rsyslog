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
template(name="dynakey" type="string" string="%$!dynaKey%")

local4.* {
        set $!dynaKey = "myDynaKey";
        action(type="omhiredis"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="set"
                dynakey="on"
                key="dynakey"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

# Should get nothing
redis_command "GET myDynaKey" > $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

# Should get ' msgnum:00000001:'
redis_command "GET myDynaKey" >> $RSYSLOG_OUT_LOG

# The first get is before inserting, the second is after
export EXPECTED="/usr/bin/redis-cli

/usr/bin/redis-cli
 msgnum:00000001:"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
