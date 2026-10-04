#!/bin/bash
# Verify that YAML config can opt one main queue into mutex contention
# diagnostics. A multi-connection imtcp workload must fully drain and report
# the metric keys, proving the YAML setting reaches the shared queue parameter
# backend without depending on a particular scheduler's contention level.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin impstats

export NUMMESSAGES=20000
export STATSFILE="$RSYSLOG_DYNNAME.stats"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/impstats/.libs/impstats"'
add_yaml_conf "    log.file: \"${STATSFILE}\""
add_yaml_conf '    interval: 1'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'mainqueue:'
add_yaml_conf '  queue.workerThreads: 4'
add_yaml_conf '  queue.workerThreadMinimumMessages: 1'
add_yaml_conf '  queue.dequeueBatchSize: 1'
add_yaml_conf '  queue.mutexContentionStats: on'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
add_yaml_conf '    workerThreads: 4'
add_yaml_conf '    ruleset: main'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg:F,58:2%\\n"'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      if (\$msg contains \"msgnum:\") then action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\" template=\"outfmt\")"

startup
tcpflood -c16 -m"$NUMMESSAGES"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
wait_file_lines "$STATSFILE" 1
shutdown_when_empty
wait_shutdown
content_check --regex --output-results \
	'main Q: origin=core.queue .*mutex\.contention=[0-9][0-9]* .*mutex\.wait_ns=[0-9][0-9]*' "$STATSFILE"
seq_check
exit_test
