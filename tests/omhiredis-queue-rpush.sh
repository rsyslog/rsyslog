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
                name="omhiredis-queue-rpush"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="queue"
                userpush="on"
                key="myKey"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'
startup

# Inject 5 messages
injectmsg 1 5

shutdown_when_empty
wait_shutdown

redis_command "LLEN myKey" > $RSYSLOG_OUT_LOG
# try to get 6 (should get only 5)
redis_command "LPOP myKey 6" >> $RSYSLOG_OUT_LOG

# Messages should be retrieved in order (as they were inserted using RPUSH)
export EXPECTED="5
 msgnum:00000001:
 msgnum:00000002:
 msgnum:00000003:
 msgnum:00000004:
 msgnum:00000005:"

cmp_exact $RSYSLOG_OUT_LOG

content_check "omhiredis[omhiredis-queue-rpush]: trying connect to '127.0.0.1'" ${RSYSLOG_DYNNAME}.started

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
