#!/bin/bash
# Verify that YAML passes pmnormalize's debug parameters and directs liblognorm
# trace output to its configured file. A non-empty trace file is the oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin pmnormalize
DEBUG_LOG="${RSYSLOG_DYNNAME}.yaml-pmnormalize-debug.log"
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '  - load: "../plugins/pmnormalize/.libs/pmnormalize"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: normalize'
add_yaml_conf 'parsers:'
add_yaml_conf '  - name: custom.pmnormalize'
add_yaml_conf '    type: pmnormalize'
add_yaml_conf '    debug: on'
add_yaml_conf '    debugFile: "'$DEBUG_LOG'"'
add_yaml_conf '    rule: "rule=:<%pri:number%> %fromhost-ip:ipv4% %hostname:word% %syslogtag:char-to:\\x3a%: %msg:rest%"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: normalize'
add_yaml_conf '    parser: custom.pmnormalize'
add_yaml_conf '    statements:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -M "\"<189> 127.0.0.1 ubuntu tag1: this is a test message\""
shutdown_when_empty
wait_shutdown
if [ ! -s "$RSYSLOG_OUT_LOG" ]; then
	echo "expected pmnormalize to parse the test message"
	error_exit 1
fi
if [ ! -s "$DEBUG_LOG" ]; then
	echo "expected liblognorm trace output in $DEBUG_LOG"
	error_exit 1
fi
exit_test
