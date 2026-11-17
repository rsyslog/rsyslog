#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# add 2016-12-07 by Pascal Withopf, released under ASL 2.0
# Both the built-in rsyslog.pmnull instance and a configured instance with no
# overrides must use the documented facility 1/severity 5 (PRI 13), regardless
# of the input's PRI. Each input uses only its named parser, so the configured
# parser cannot hide the built-in path. Exact output after synchronized shutdown
# is the oracle; rawmsg must also retain the original input. Port-file readiness
# gates the control sender; each one-line wait proves delivery before shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin pmnull
control_portfile="${RSYSLOG_DYNNAME}.control.port"
control_output="${RSYSLOG_DYNNAME}.control.log"
if [ "${PMNULL_TEST_YAML:-0}" = 1 ]; then
	generate_conf --yaml-only
	add_yaml_conf 'modules:'
	add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
	add_yaml_conf '  - load: "../plugins/pmnull/.libs/pmnull"'
	add_yaml_conf 'parsers:'
	add_yaml_conf '  - name: custom.pmnull'
	add_yaml_conf '    type: pmnull'
	add_yaml_conf 'templates:'
	add_yaml_conf '  - name: pmnullOutput'
	add_yaml_conf '    type: string'
	add_yaml_conf '    string: "%pri%|%syslogfacility%|%syslogseverity%|%rawmsg%\n"'
	add_yaml_conf 'inputs:'
	add_yaml_conf '  - type: imtcp'
	add_yaml_conf '    address: "127.0.0.1"'
	add_yaml_conf '    port: "0"'
	add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
	add_yaml_conf '    ruleset: builtin'
	add_yaml_conf '  - type: imtcp'
	add_yaml_conf '    address: "127.0.0.1"'
	add_yaml_conf '    port: "0"'
	add_yaml_conf "    listenPortFileName: \"${control_portfile}\""
	add_yaml_conf '    ruleset: configured'
	add_yaml_conf 'rulesets:'
	add_yaml_conf '  - name: builtin'
	add_yaml_conf '    parser: rsyslog.pmnull'
	add_yaml_conf '    statements:'
	add_yaml_conf '      - type: omfile'
	add_yaml_conf "        file: \"${RSYSLOG_OUT_LOG}\""
	add_yaml_conf '        template: pmnullOutput'
	add_yaml_conf '  - name: configured'
	add_yaml_conf '    parser: custom.pmnull'
	add_yaml_conf '    statements:'
	add_yaml_conf '      - type: omfile'
	add_yaml_conf "        file: \"${control_output}\""
	add_yaml_conf '        template: pmnullOutput'
else
	generate_conf
	add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/pmnull/.libs/pmnull")
parser(name="custom.pmnull" type="pmnull")
template(name="pmnullOutput" type="string" string="%pri%|%syslogfacility%|%syslogseverity%|%rawmsg%\n")
input(type="imtcp" address="127.0.0.1" port="0"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="builtin")
input(type="imtcp" address="127.0.0.1" port="0"
 listenPortFileName="'$control_portfile'" ruleset="configured")
ruleset(name="builtin" parser="rsyslog.pmnull") {
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="pmnullOutput")
}
ruleset(name="configured" parser="custom.pmnull") {
 action(type="omfile" file="'$control_output'" template="pmnullOutput")
}
'
fi
startup
wait_file_exists "$control_portfile"
control_port=$(cat "$control_portfile")
tcpflood -m1 -M '"<189>pmnull defaults input"'
tcpflood -p "$control_port" -m1 -M '"<189>pmnull defaults input"'
wait_file_lines "$RSYSLOG_OUT_LOG" 1
wait_file_lines "$control_output" 1
shutdown_when_empty
wait_shutdown
export EXPECTED='13|1|5|<189>pmnull defaults input'
cmp_exact "$control_output"
echo 'configured pmnull default control matched PRI/facility/severity 13/1/5'
cmp_exact
exit_test
