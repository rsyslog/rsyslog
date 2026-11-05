#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify that the YAML frontend passes excludeSubnets to mmanon and preserves
# an in-subnet literal while anonymizing a nonmatch from the same message. The
# output file is the runtime oracle for parity with RainerScript configuration.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmanon
require_yaml_support

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/mmanon/.libs/mmanon"'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'${RSYSLOG_DYNNAME}'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: mmanon'
add_yaml_conf '        ipv4.bits: "32"'
add_yaml_conf '        ipv6.enable: "off"'
add_yaml_conf '        excludeSubnets:'
add_yaml_conf '          - "192.0.2.0/24"'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'${RSYSLOG_OUT_LOG}'"'
add_yaml_conf '        template: outfmt'

startup
tcpflood -m1 -M '"<129>Mar 10 01:00:00 172.20.245.8 tag: yaml 192.0.2.1 and 192.0.3.1"'
shutdown_when_empty
wait_shutdown
export EXPECTED=' yaml 192.0.2.1 and 0.0.0.0'
cmp_exact
exit_test
