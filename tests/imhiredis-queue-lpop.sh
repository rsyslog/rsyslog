#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# added 2023-04-20 by Théo Bertin, released under ASL 2.0
# Omitting server must retain the explicit port on the default loopback host.
# The harness starts Redis on an isolated random port. Wait for all three rows
# before shutdown, then verify their LPOP order; the standard testbench timeout
# bounds a connection to the wrong port without a fixed-delay success oracle.
## Uncomment for debugging
#export RS_REDIR=-d

. ${srcdir:=.}/diag.sh init

start_redis
REDIS_TEST_KEY="${RSYSLOG_DYNNAME}_port"

redis_command "RPUSH $REDIS_TEST_KEY message1"
redis_command "RPUSH $REDIS_TEST_KEY message2"
redis_command "RPUSH $REDIS_TEST_KEY message3"

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/imhiredis/.libs/imhiredis")
template(name="outfmt" type="string" string="%$/num% %msg%\n")


input(type="imhiredis"
        port="'$REDIS_RANDOM_PORT'"
        key="'$REDIS_TEST_KEY'"
        mode="queue"
        uselpop="on"
        ruleset="redis")

ruleset(name="redis") {
  set $/num = cnum($/num + 1);
  action(type="omfile"
            file="'$RSYSLOG_OUT_LOG'"
            template="outfmt")
}

'
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
shutdown_when_empty
wait_shutdown

stop_redis

# Exactly three messages, in the same order as they were pushed.
export EXPECTED='1 message1
2 message2
3 message3'
cmp_exact "$RSYSLOG_OUT_LOG"

# Removes generated configuration file, log and pid files
cleanup_redis

exit_test
