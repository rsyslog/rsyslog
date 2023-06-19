#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
# export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis
REDIS_PASSWORD="mySuperSecretPasswd"

# Set a password on started Redis server
redis_command "CONFIG SET requirepass ${REDIS_PASSWORD}"

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/omhiredis/.libs/omhiredis")
template(name="outfmt" type="string" string="%msg%")

local4.* {
        action(type="omhiredis"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                serverpassword="'${REDIS_PASSWORD}'"
                mode="set"
                key="outKey"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

# Client MUST authentiate here!
redis_command "AUTH ${REDIS_PASSWORD} \n GET outKey" > $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

# Should get ' msgnum:00000001:'
redis_command "AUTH ${REDIS_PASSWORD} \n GET outKey" >> $RSYSLOG_OUT_LOG

# the "OK" replies are for authentication of the redis cli client
export EXPECTED="/usr/bin/redis-cli
OK

/usr/bin/redis-cli
OK
 msgnum:00000001:"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
