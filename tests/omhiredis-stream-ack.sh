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
                name="omhiredis-stream-ack"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="stream"
                key="outStream"
                stream.ack="on"
                stream.keyAck="inStream"
                stream.groupAck="group"
                stream.indexAck="1-0"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

redis_command "XGROUP CREATE inStream group 0 MKSTREAM" > $RSYSLOG_OUT_LOG
redis_command "XADD inStream 1-0 key value" >> $RSYSLOG_OUT_LOG
redis_command "XREADGROUP GROUP group consumerName COUNT 1 STREAMS inStream >" >> $RSYSLOG_OUT_LOG
redis_command "XINFO GROUPS inStream" >> $RSYSLOG_OUT_LOG

startup

# Inject 1 message
injectmsg 1 1

shutdown_when_empty
wait_shutdown

redis_command "XINFO GROUPS inStream" >> $RSYSLOG_OUT_LOG

# 1. create group and stream
# 2. add entry to stream (with index 1-0)
# 3. read it from a consumer group -> index is now pending
# 4. show group infos -> pending shows 1 pending entry
# 4.2. start Rsyslog and send message -> omhiredis acknowledges index 1-0 on group 'group' for stream 'inStream'
# 5. show group infos again -> pending now shows 0 pending entries
export EXPECTED="OK
1-0
inStream
1-0
key
value
name
group
consumers
1
pending
1
last-delivered-id
1-0
entries-read
1
lag
0
name
group
consumers
1
pending
0
last-delivered-id
1-0
entries-read
1
lag
0"

cmp_exact $RSYSLOG_OUT_LOG

content_check "omhiredis: no stream.outField set, using 'msg' as default" ${RSYSLOG_DYNNAME}.started
content_check "omhiredis[omhiredis-stream-ack]: trying connect to '127.0.0.1'" ${RSYSLOG_DYNNAME}.started

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
