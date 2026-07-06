#!/bin/bash
# Verify issue #1324 through YAML config parsing: an imtcp input bound to a
# YAML-defined ruleset without its own non-direct queue emits the same warning
# as the RainerScript path, while a YAML ruleset with queue.type="LinkedList"
# stays warning-free. The oracle is rsyslogd -N1 output, so no listener is
# started and no socket readiness timing is involved.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")
'
rm -f "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'${RSYSLOG_DYNNAME}'.tcpflood_port"'
add_yaml_conf '    ruleset: shared'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: shared'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'${RSYSLOG_OUT_LOG}'")'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.warn.log" 2>&1
if [ $? -ne 0 ]; then
	echo "FAIL: expected YAML config validation to continue after ruleset queue warning"
	cat "${RSYSLOG_DYNNAME}.warn.log"
	error_exit 1
fi

content_check "input-bound ruleset 'shared' has no dedicated non-direct queue" \
	"${RSYSLOG_DYNNAME}.warn.log"

generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")
'
rm -f "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'${RSYSLOG_DYNNAME}'.tcpflood_port"'
add_yaml_conf '    ruleset: queued'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: queued'
add_yaml_conf '    queue.type: LinkedList'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'${RSYSLOG_OUT_LOG}'")'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.quiet.log" 2>&1
if [ $? -ne 0 ]; then
	echo "FAIL: expected YAML config validation to accept queued ruleset binding"
	cat "${RSYSLOG_DYNNAME}.quiet.log"
	error_exit 1
fi

check_not_present "input-bound ruleset 'queued' has no dedicated non-direct queue" \
	"${RSYSLOG_DYNNAME}.quiet.log"

exit_test
