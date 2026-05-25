#!/bin/bash
# Tests imfifo input parameters configured via YAML.
#
# The Setup:
# A YAML-only configuration loads imfifo, creates a FIFO input with file, tag,
# severity, facility, and ruleset parameters, and routes messages through an
# omfile action.
#
# The Oracle:
# Success is proven by writing two newline-delimited messages to the FIFO,
# waiting until both appear in the output file, and comparing the exact facility,
# severity, tag, and message text produced by the YAML-configured input.
#
# Timing / Wait Justification:
# wait_file_lines synchronizes on the expected output count before shutdown so
# the asynchronous FIFO reader and action queue have completed their work.
#
# Added 2026-05-26, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imfifo

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imfifo/.libs/imfifo"'
add_yaml_conf ''
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%syslogfacility-text%.%syslogseverity-text% %programname%: %msg%\n"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imfifo'
add_yaml_conf '    file: "'$RSYSLOG_DYNNAME'.fifo"'
add_yaml_conf '    tag: "yamlpro:"'
add_yaml_conf '    severity: "notice"'
add_yaml_conf '    facility: "local4"'
add_yaml_conf '    ruleset: "main"'
add_yaml_conf ''
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")'

rm -f $RSYSLOG_DYNNAME.fifo
mkfifo $RSYSLOG_DYNNAME.fifo
startup

(
  echo "yaml message 1" > $RSYSLOG_DYNNAME.fifo
  echo "yaml message 2" > $RSYSLOG_DYNNAME.fifo
) &
PIPE_WRITER_PID=$!

wait_file_lines "$RSYSLOG_OUT_LOG" 2 60
shutdown_when_empty
wait_shutdown
wait $PIPE_WRITER_PID
rm -f $RSYSLOG_DYNNAME.fifo

echo "local4.notice yamlpro: yaml message 1
local4.notice yamlpro: yaml message 2" | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi

exit_test
