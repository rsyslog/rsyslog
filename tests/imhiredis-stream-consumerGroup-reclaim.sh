#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/imhiredis/.libs/imhiredis")
template(name="outfmt" type="string" string="%$/num% %$!msg%\n")


input(type="imhiredis"
        server="127.0.0.1"
        port="'$REDIS_RANDOM_PORT'"
        key="mystream"
        mode="stream"
        stream.consumerGroup="mygroup"
        stream.consumerName="myName"
        stream.autoclaimIdleTime="5000" #5 seconds
        ruleset="redis")

ruleset(name="redis") {
  set $/num = cnum($/num + 1);
  action(type="omfile"
            file="'$RSYSLOG_OUT_LOG'"
            template="outfmt")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

redis_command "XADD mystream * msg message1"
redis_command "XADD mystream * msg message2"
redis_command "XADD mystream * msg message3"
redis_command "XADD mystream * msg message4"
redis_command "XADD mystream * msg message5"
redis_command "XADD mystream * msg message6"

redis_command "XGROUP CREATE mystream mygroup 0-0"
# Read and claim message1 and message2
redis_command "XREADGROUP GROUP mygroup otherConsumer COUNT 2 STREAMS mystream >"
rst_msleep 5500
# Read and claim message3 and message4
redis_command "XREADGROUP GROUP mygroup otherConsumer COUNT 2 STREAMS mystream >"

startup

shutdown_when_empty
wait_shutdown

output="$(redis_command 'hello 3\nXINFO groups mystream' | grep 'pending')"

if [ -z "$output" ]; then
    echo "Could not get group result from redis, cannot tell if entries ware acknowledged!"
    error_exit 1
fi

# Should still have 2 pending messages: message3 and message4
if ! echo "$output" | grep -q "pending 2"; then
    echo "ERROR: entries weren't acknowledged!"
    echo "ERROR: output from Redis is '$output'"
    echo "ERROR: expected 'pending 2'"
    error_exit 1
fi

stop_redis

# Should reclaim message1 and message2
# then claim and acknowledge message5 and message6 normally
content_check '1 message1'
content_check '2 message2'
content_check '3 message5'
content_check '4 message6'

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
