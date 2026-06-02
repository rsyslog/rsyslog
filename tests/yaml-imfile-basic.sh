#!/bin/bash
# Tests imfile input configured via YAML.
# Uses inputfilegen to produce messages into a watched file; imfile reads
# them and routes through a YAML-defined ruleset to the output file.
# Mirrors imfile-basic.sh using YAML configuration.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imfile
export NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imfile/.libs/imfile"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

inputs:
  - type: imfile
    file: "./RSYSLOG_DYNNAME.input"
    tag: "file:"
    ruleset: "main"

rulesets:
  - name: main
    script: |
      if $msg contains "msgnum:" then
        action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF
sed -i \
    -e "s|RSYSLOG_DYNNAME|${RSYSLOG_DYNNAME}|g" \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

touch "${RSYSLOG_DYNNAME}.input"
startup
./inputfilegen -m $NUMMESSAGES >> "${RSYSLOG_DYNNAME}.input"
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
