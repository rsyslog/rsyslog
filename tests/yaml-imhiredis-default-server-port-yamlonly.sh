#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Omitting server in YAML must preserve an explicit port for the default
# loopback host. The exact three ordered Redis records prove delivery through
# that port. A bounded row-count predicate waits for consumption before
# shutdown; the standard testbench timeout catches an ignored port.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imhiredis ../contrib/imhiredis
start_redis
REDIS_TEST_KEY="${RSYSLOG_DYNNAME}_port"
redis_command "RPUSH $REDIS_TEST_KEY message1 message2 message3"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../contrib/imhiredis/.libs/imhiredis"'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%\n"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imhiredis'
add_yaml_conf '    port: "'$REDIS_RANDOM_PORT'"'
add_yaml_conf '    key: "'$REDIS_TEST_KEY'"'
add_yaml_conf '    mode: queue'
add_yaml_conf '    useLPop: "on"'
add_yaml_conf '    ruleset: redis'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: redis'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")'
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
shutdown_when_empty
wait_shutdown
stop_redis
export EXPECTED='message1
message2
message3'
cmp_exact "$RSYSLOG_OUT_LOG"
cleanup_redis
exit_test
