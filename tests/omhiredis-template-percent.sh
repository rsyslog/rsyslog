#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# added 2026-11-09 by Rainer Gerhards, released under ASL 2.0
#
# Intent: omhiredis template mode must send '%' characters coming from the
# message literally. hiredis parses the rendered template printf-style; before
# the fix a message containing "%s" or "%d" was expanded as a conversion and
# read nonexistent variadic arguments (garbage value or crash).
#
# Setup: template "SET pctkey %msg%" in mode="template". One message whose
# MSG part is a single token containing several '%' conversions is injected.
# Oracle: after shutdown, GET pctkey must return the token byte-for-byte,
# proving the '%' sequences were stored literally and space-splitting of the
# command still works (SET, key and value arrive as separate arguments).
## Uncomment for debugging
# export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/omhiredis/.libs/omhiredis")
template(name="redis_command" type="string" string="SET pctkey %msg%")

local4.* {
        action(type="omhiredis"
                name="omhiredis-template-percent"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="template"
                template="redis_command")
        stop
}
'

startup
injectmsg_literal '<167>Mar  1 01:00:00 172.20.245.8 tag: pct%s%d%x%%end'
shutdown_when_empty
wait_shutdown

redis_command "GET pctkey" > $RSYSLOG_OUT_LOG

export EXPECTED='pct%s%d%x%%end'
cmp_exact $RSYSLOG_OUT_LOG

stop_redis
cleanup_redis
exit_test
