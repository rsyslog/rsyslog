#!/bin/bash
# Verify YAML parsing for modern imtcp/imudp allowedSender parameters. The
# module default denies localhost while input-level overrides allow it; blocked
# inputs prove the module default is active. Allowed paths synchronize on exact
# output line counts, and blocked paths are checked by absent output files.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imudp

export NUMMESSAGES=3
TCP_ALLOW_LOG="${RSYSLOG_DYNNAME}.yaml-tcp-allow.log"
UDP_ALLOW_LOG="${RSYSLOG_DYNNAME}.yaml-udp-allow.log"
TCP_BLOCK_LOG="${RSYSLOG_DYNNAME}.yaml-tcp-block.must-not-exist"
UDP_BLOCK_LOG="${RSYSLOG_DYNNAME}.yaml-udp-block.must-not-exist"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    allowedSender: ["128.66.0.0/16"]'
add_yaml_conf '  - load: "../plugins/imudp/.libs/imudp"'
add_yaml_conf '    allowedSender: ["128.66.0.0/16"]'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.yaml_tcp_block_port\""
add_yaml_conf '    ruleset: tcp_block'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.yaml_tcp_allow_port\""
add_yaml_conf '    allowedSender: ["127.0.0.1/16", "[::1]"]'
add_yaml_conf '    ruleset: tcp_allow'
add_yaml_conf '  - type: imudp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.yaml_udp_block_port\""
add_yaml_conf '    ruleset: udp_block'
add_yaml_conf '  - type: imudp'
add_yaml_conf '    address: "127.0.0.1"'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.yaml_udp_allow_port\""
add_yaml_conf '    allowedSender: ["127.0.0.1/16"]'
add_yaml_conf '    ruleset: udp_allow'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg:F,58:2%\n"'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: tcp_block'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" template=\"outfmt\" file=\"${TCP_BLOCK_LOG}\")"
add_yaml_conf '  - name: tcp_allow'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" template=\"outfmt\" file=\"${TCP_ALLOW_LOG}\")"
add_yaml_conf '  - name: udp_block'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" template=\"outfmt\" file=\"${UDP_BLOCK_LOG}\")"
add_yaml_conf '  - name: udp_allow'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" template=\"outfmt\" file=\"${UDP_ALLOW_LOG}\")"
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\")"

startup

assign_file_content TCP_BLOCK_PORT "${RSYSLOG_DYNNAME}.yaml_tcp_block_port"
assign_file_content TCP_ALLOW_PORT "${RSYSLOG_DYNNAME}.yaml_tcp_allow_port"
assign_file_content UDP_BLOCK_PORT "${RSYSLOG_DYNNAME}.yaml_udp_block_port"
assign_file_content UDP_ALLOW_PORT "${RSYSLOG_DYNNAME}.yaml_udp_allow_port"

tcpflood --check-only -p"$TCP_BLOCK_PORT" -m1 -M "msgnum:yaml-tcp-block"
tcpflood -p"$TCP_ALLOW_PORT" -m"$NUMMESSAGES" -M "msgnum:yaml-tcp-allow"
tcpflood -Tudp -p"$UDP_BLOCK_PORT" -m1 -M "msgnum:yaml-udp-block"
tcpflood -Tudp -p"$UDP_ALLOW_PORT" -m"$NUMMESSAGES" -M "msgnum:yaml-udp-allow"

wait_file_lines "$TCP_ALLOW_LOG" "$NUMMESSAGES" 100
wait_file_lines "$UDP_ALLOW_LOG" "$NUMMESSAGES" 100
shutdown_when_empty
wait_shutdown

check_file_not_exists "$TCP_BLOCK_LOG"
check_file_not_exists "$UDP_BLOCK_LOG"
exit_test
