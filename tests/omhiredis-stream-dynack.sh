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

template(name="redis-key" type="string" string="%$.redis!key%")
template(name="redis-group" type="string" string="%$.redis!group%")
template(name="redis-index" type="string" string="%$.redis!index%")

local4.* {
        set $.redis!key = "inputStream";
        set $.redis!group = "myGroup";
        set $.redis!index = "2-0";
        action(type="omhiredis"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="stream"
                key="outStream"
                stream.ack="on"
                stream.dynaKeyAck="on"
                stream.dynaGroupAck="on"
                stream.dynaIndexAck="on"
                stream.keyAck="redis-key"
                stream.groupAck="redis-group"
                stream.indexAck="redis-index"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

redis_command "XGROUP CREATE inputStream myGroup 0 MKSTREAM" > $RSYSLOG_OUT_LOG
redis_command "XADD inputStream 2-0 key value" >> $RSYSLOG_OUT_LOG
redis_command "XREADGROUP GROUP myGroup consumerName COUNT 1 STREAMS inputStream >" >> $RSYSLOG_OUT_LOG
redis_command "XINFO GROUPS inputStream" >> $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

redis_command "XINFO GROUPS inputStream" >> $RSYSLOG_OUT_LOG

# 1. create group and stream
# 2. add entry to stream (with index 2-0)
# 3. read it from a consumer group -> index is now pending
# 4. show group infos -> pending shows 1 pending entry
# 4.2. start Rsyslog and send message -> omhiredis acknowledges index 2-0 on group 'myGroup' for stream 'inputStream'
# 5. show group infos again -> pending now shows 0 pending entries
export EXPECTED="/usr/bin/redis-cli
OK
/usr/bin/redis-cli
2-0
/usr/bin/redis-cli
inputStream
2-0
key
value
/usr/bin/redis-cli
name
myGroup
consumers
1
pending
1
last-delivered-id
2-0
entries-read
1
lag
0
/usr/bin/redis-cli
name
myGroup
consumers
1
pending
0
last-delivered-id
2-0
entries-read
1
lag
0"

cmp_exact $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
