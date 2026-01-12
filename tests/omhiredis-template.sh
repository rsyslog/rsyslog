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
template(name="redis_command" type="string" string="INCRBY counter 3")

local4.* {
        action(type="omhiredis"
                name="omhiredis-template"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="template"
                template="redis_command")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'

# Should get 'none'
redis_command "TYPE counter" > $RSYSLOG_OUT_LOG

startup

# Inject 3 messages
injectmsg 1 3

# Should get 'string'
redis_command "TYPE counter" >> $RSYSLOG_OUT_LOG

shutdown_when_empty
wait_shutdown

# Should get '9'
redis_command "GET counter" >> $RSYSLOG_OUT_LOG

# The first get is before inserting, the second is after
export EXPECTED="none
string
9"

cmp_exact $RSYSLOG_OUT_LOG

content_check "omhiredis[omhiredis-template]: trying connect to '127.0.0.1'" ${RSYSLOG_DYNNAME}.started

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
