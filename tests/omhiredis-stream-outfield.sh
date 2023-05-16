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
                stream.outField="custom_field"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

startup

# Inject 2 messages
injectmsg 1 2

shutdown_when_empty
wait_shutdown

# Should get '2'
redis_command "XLEN outStream" >> $RSYSLOG_OUT_LOG
# Should get 2 entries
redis_command "XREAD COUNT 3 STREAMS outStream 0" >> $RSYSLOG_OUT_LOG

# Cannot check for full reply as it includes entries' unix timestamp
content_count_check "outStream" 1 $RSYSLOG_OUT_LOG
content_count_check " msgnum:00000001:" 1 $RSYSLOG_OUT_LOG
content_count_check " msgnum:00000002:" 1 $RSYSLOG_OUT_LOG
# 2 data should be inserted in the 'custom_field' field inside every message
content_count_check "custom_field" 2 $RSYSLOG_OUT_LOG
content_count_check "msg" 2 $RSYSLOG_OUT_LOG
content_count_check "msgnum" 2 $RSYSLOG_OUT_LOG

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
