#!/bin/bash
# Tests basic YAML configuration support:
#   - module loading via YAML modules: section
#   - input definition via YAML inputs: section
#   - action definition in a ruleset script: block
#   - a YAML file included from the RainerScript testbench preamble
#
# The testbench instrumentation (imdiag, timeouts) lives in the generated
# .conf file; the test config itself lives in a .yaml file that is pulled in
# via include().  This exercises both YAML parsing and cross-format includes.
#
# Added 2025 by contributors, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
# Pull in the YAML part of the config
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'
# Write the YAML config fragment
cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    script: |
      :msg, contains, "msgnum:" action(type="omfile"
          template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF
# Substitute shell variables in the YAML (RSYSLOG_OUT_LOG is not expanded
# inside single-quoted HEREDOC, so we use sed)
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"
add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
