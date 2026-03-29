#!/bin/bash
# Tests that the YAML-only testbench mode works end-to-end:
#   - generate_conf --yaml-only writes a pure YAML preamble using
#     testbench_modules: for the imdiag setup (no RainerScript)
#   - test-specific modules go in a standard modules: section
#   - add_yaml_imdiag_input adds the imdiag input for startup detection
#   - rsyslogd starts and processes messages using only the YAML loader
#
# This test intentionally avoids any RainerScript or legacy-style directives
# so that it exercises the yaml-only testbench path.
#
# Added 2025 by contributors, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf --yaml-only
# Test-specific modules go in a standard modules: section.
# Testbench infrastructure (imdiag) is in testbench_modules: written by the preamble.
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf "    string: \"%msg:F,58:2%\\n\""
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_imdiag_input
add_yaml_conf "  - type: imtcp"
add_yaml_conf "    port: \"0\""
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
add_yaml_conf "    ruleset: main"
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf "      :msg, contains, \"msgnum:\" action(type=\"omfile\""
add_yaml_conf "          template=\"outfmt\" file=\"${RSYSLOG_OUT_LOG}\")"
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
