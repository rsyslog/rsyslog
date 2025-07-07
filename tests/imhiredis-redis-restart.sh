#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

# Start redis once to be able to generate configuration
start_redis

redis_command "RPUSH mykey message1"
redis_command "RPUSH mykey message2"
redis_command "RPUSH mykey message3"

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/imhiredis/.libs/imhiredis")
template(name="outfmt" type="string" string="%$/num% %msg%\n")


input(type="imhiredis"
        server="127.0.0.1"
        port="'$REDIS_RANDOM_PORT'"
        key="mykey"
        mode="queue"
        ruleset="redis")

ruleset(name="redis") {
  set $/num = cnum($/num + 1);
  action(type="omfile"
            file="'$RSYSLOG_OUT_LOG'"
            template="outfmt")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

wait_content '3 message1'

# Stop Redis and wait a short moment for imhiredis to notice Redis went down
stop_redis
rst_msleep 1500
start_redis

redis_command "RPUSH mykey message4"
redis_command "RPUSH mykey message5"
redis_command "RPUSH mykey message6"

wait_content '4 message6'

shutdown_when_empty
wait_shutdown

stop_redis

content_check '1 message3'
content_check '2 message2'
content_check '3 message1'
content_check 'sleeping 10 seconds before retrying'
content_check '4 message6'
content_check '5 message5'
content_check '6 message4'

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
