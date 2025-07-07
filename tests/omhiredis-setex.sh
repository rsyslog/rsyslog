#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
# export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init
EXPIRATION="3"

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
                mode="set"
                key="outKey"
                expiration="'$EXPIRATION'"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

# Should get nothing
redis_command "GET outKey" > $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

# Should get the remaining expiration time (over -1, under 5)
ttl=$(redis_command "TTL outKey" | sed '1d')

# Should get ' msgnum:00000001:'
redis_command "GET outKey" >> $RSYSLOG_OUT_LOG

sleep $EXPIRATION

# Should get nothing
redis_command "GET outKey" >> $RSYSLOG_OUT_LOG

if [ $ttl -lt 0 ] || [ $ttl -gt $EXPIRATION ]; then
    echo "ERROR: expiration is not in [0:$EXPIRATION] -> $ttl"
    error_exit 1
fi

# The first get is before inserting
# The third is while the key is still valid
# The fourth is after the key expired
export EXPECTED="/usr/bin/redis-cli

/usr/bin/redis-cli
 msgnum:00000001:
/usr/bin/redis-cli
"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
