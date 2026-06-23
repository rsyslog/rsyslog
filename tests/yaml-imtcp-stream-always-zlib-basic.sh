#!/bin/bash
# added 2026-06-22 by Codex, released under ASL 2.0
# Verifies YAML parsing of imtcp stream compression input parameters. The
# receiver is configured via YAML with compression.mode, compression.driver,
# compression.maxExpansionRatio, and compression.maxDecompressedBytesPerReceive;
# the sender uses omfwd to emit a zlib stream. The oracle is a complete,
# ordered sequence at the receiver.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=20000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf "    string: \"%msg:F,58:2%\\n\""
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
add_yaml_conf '    compression.mode: "stream:always"'
add_yaml_conf '    compression.driver: "zlib"'
add_yaml_conf '    compression.maxExpansionRatio: "2048"'
add_yaml_conf '    compression.maxDecompressedBytesPerReceive: "10485760"'
add_yaml_conf '    ruleset: main'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      :msg, contains, \"msgnum:\" action(type=\"omfile\""
add_yaml_conf "          template=\"outfmt\" file=\"${RSYSLOG_OUT_LOG}\")"
startup
export RCVR_PORT=$TCPFLOOD_PORT

generate_conf 2
add_conf '
module(load="builtin:omfwd")

*.* action(type="omfwd"
	target="127.0.0.1"
	port="'$RCVR_PORT'"
	protocol="tcp"
	compression.mode="stream:always"
	compression.driver="zlib"
	compression.stream.flushOnTXEnd="on")
' 2
startup 2

injectmsg2
shutdown_when_empty 2
wait_shutdown 2
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
