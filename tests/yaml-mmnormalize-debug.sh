#!/bin/bash
# Verify that YAML passes mmnormalize's debug parameters and directs liblognorm
# trace output to its configured file. A non-empty trace file is the oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
DEBUG_LOG="${RSYSLOG_DYNNAME}.yaml-mmnormalize-debug.log"
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '  - load: "../plugins/mmnormalize/.libs/mmnormalize"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: normalize'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: normalize'
add_yaml_conf '    statements:'
add_yaml_conf '      - type: mmnormalize'
add_yaml_conf '        useRawMsg: on'
add_yaml_conf '        debug: on'
add_yaml_conf '        debugFile: "'$DEBUG_LOG'"'
add_yaml_conf '        rule: "rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%"'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -M "\"ubuntu tag1: no longer listening on 127.168.0.1#10514\""
shutdown_when_empty
wait_shutdown
if [ ! -s "$DEBUG_LOG" ]; then
	echo "expected liblognorm trace output in $DEBUG_LOG"
	error_exit 1
fi
exit_test
