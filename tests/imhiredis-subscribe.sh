#!/usr/bin/env bash
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
## Uncomment for debugging
#export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis

# Won't be logged by Rsyslog
redis_command "PUBLISH mychannel message1"
redis_command "PUBLISH mychannel message2"
redis_command "PUBLISH mychannel message3"

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/imhiredis/.libs/imhiredis")
template(name="outfmt" type="string" string="%$/num% %msg%\n")


input(type="imhiredis"
        server="127.0.0.1"
        port="'$REDIS_RANDOM_PORT'"
        key="mychannel"
        mode="subscribe"
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

redis_command "PUBLISH mychannel message4"
redis_command "PUBLISH mychannel message5"
redis_command "PUBLISH mychannel message6"

shutdown_when_empty
wait_shutdown

stop_redis

check_not_present "message1"
check_not_present "message2"
check_not_present "message3"

content_check '1 message4'
content_check '2 message5'
content_check '3 message6'

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
