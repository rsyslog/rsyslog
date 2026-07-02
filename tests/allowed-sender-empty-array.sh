#!/bin/bash
# Verify that modern YAML allowedSender arrays cannot be empty. An empty ACL is
# a security-sensitive typo: treating it as a configured-but-NULL list would
# allow every sender. RainerScript rejects literal [] as syntax before module
# parsing, so this test covers the YAML-visible module/input scopes.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imudp

expect_config_reject() {
	local test_name="$1"
	local logfile="${RSYSLOG_DYNNAME}.${test_name}.log"

	export RS_REDIR=">\"$logfile\" 2>&1"
	if rsyslogd_config_check; then
		unset RS_REDIR
		echo "FAIL: config check accepted empty allowedSender array for $test_name"
		error_exit 1
	fi
	unset RS_REDIR
	content_check "allowedSender array must not be empty" "$logfile"
}

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    allowedSender: []'
expect_config_reject "yaml-imtcp-module"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcp_port\""
add_yaml_conf '    allowedSender: []'
expect_config_reject "yaml-imtcp-input"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imudp/.libs/imudp"'
add_yaml_conf '    allowedSender: []'
expect_config_reject "yaml-imudp-module"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imudp/.libs/imudp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imudp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.udp_port\""
add_yaml_conf '    allowedSender: []'
expect_config_reject "yaml-imudp-input"

exit_test
