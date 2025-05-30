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
                stream.del="on"
                stream.keyAck="inStream"
                stream.indexAck="1-0"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

redis_command "XADD inStream 1-0 key value" >> $RSYSLOG_OUT_LOG
redis_command "XREAD STREAMS inStream 0" >> $RSYSLOG_OUT_LOG
redis_command "XLEN inStream" >> $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

redis_command "XLEN inStream" >> $RSYSLOG_OUT_LOG

# 2. add entry to stream (with index 1-0)
# 3. read it -> index is read (but not deleted)
# 4. show stream length -> should be 1
# 4.2. start Rsyslog and send message -> omhiredis deletes index 1-0 on stream 'inStream'
# 5.  show stream length again -> should be 0
export EXPECTED="/usr/bin/redis-cli
1-0
/usr/bin/redis-cli
inStream
1-0
key
value
/usr/bin/redis-cli
1
/usr/bin/redis-cli
0"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
