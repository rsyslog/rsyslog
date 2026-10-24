#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
# Verify the subscription ACK and all three published messages exactly. Keep
# redis-cli in command-argument subscription mode: piped command input can
# reach EOF and disconnect before publishing. The ACK is the readiness oracle;
# twelve output lines prove all messages arrived before owned-client cleanup.
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
                name="omhiredis-publish"
                server="127.0.0.1"
                serverport="'$REDIS_RANDOM_PORT'"
                mode="publish"
                key="myChannel"
                template="outfmt")
        stop
}

action(type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'" template="outfmt")
'
subscriber_pid=
cleanup_subscriber() {
	if [ -n "$subscriber_pid" ]; then
		kill "$subscriber_pid" 2>/dev/null || true
		wait "$subscriber_pid" 2>/dev/null || true
		subscriber_pid=
	fi
}
trap cleanup_subscriber EXIT
redis-cli -p "$REDIS_RANDOM_PORT" --raw SUBSCRIBE myChannel > "$RSYSLOG_OUT_LOG" &
subscriber_pid=$!
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3

startup

# Inject 3 messages
injectmsg 1 3

shutdown_when_empty
wait_shutdown
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 12

# A live subscriber is expected to exit only when we terminate it explicitly.
kill "$subscriber_pid" || error_exit 1
subscriber_status=0
wait "$subscriber_pid" || subscriber_status=$?
subscriber_pid=
if [ "$subscriber_status" -ne 143 ]; then
	printf 'Unexpected subscriber termination status: %s\n' "$subscriber_status"
	error_exit 1
fi

# The SUBSCRIBE command gets the result of the subscribing and the 3 subsequent messages published by omhiredis
export EXPECTED="subscribe
myChannel
1
message
myChannel
 msgnum:00000001:
message
myChannel
 msgnum:00000002:
message
myChannel
 msgnum:00000003:"

cmp_exact $RSYSLOG_OUT_LOG

content_check "omhiredis[omhiredis-publish]: trying connect to '127.0.0.1'" ${RSYSLOG_DYNNAME}.started

stop_redis

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
