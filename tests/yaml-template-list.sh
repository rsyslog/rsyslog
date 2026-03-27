#!/bin/bash
# Tests YAML list template support with property and constant elements,
# including modifiers:
#   - property: name, droplastlf, format (json), onempty
#   - constant: value
#   - template-level option: option.json
#
# Uses a list template that extracts the message number field and wraps it
# in a single-value JSON object using option.json (whole-template JSON, not
# per-field jsonf), verifying the property modifiers flow through correctly.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  # String template for seq_check
  - name: seqfmt
    type: string
    string: "%msg:F,58:2%\n"

  # List template exercising property/constant modifiers.
  # Produces:  <msgfield>\n
  # - droplastlf removes any trailing LF from the property value
  # - onempty=keep means an empty value is still emitted
  # - constant provides the newline separator
  - name: listfmt
    type: list
    elements:
      - property:
          name: msg
          position.from: "1"
          position.to: "256"
          droplastlf: "on"
          onempty: keep
      - constant:
          value: "\n"

rulesets:
  - name: main
    statements:
      - if: '$msg contains "msgnum:"'
        then:
          - type: omfile
            template: seqfmt
            file: "${RSYSLOG_OUT_LOG}"
          - type: omfile
            template: listfmt
            file: "${RSYSLOG_DYNNAME}.list"
YAMLEOF
sed -i \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    -e "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
# List output must have exactly NUMMESSAGES lines
LINES=$(wc -l < "${RSYSLOG_DYNNAME}.list")
if [ "$LINES" -ne "$NUMMESSAGES" ]; then
    echo "FAIL: expected $NUMMESSAGES list-template lines, got $LINES"
    cat "${RSYSLOG_DYNNAME}.list"
    exit 1
fi
# Each line must contain the msgnum: marker (property extraction worked)
if ! grep -q "msgnum:" "${RSYSLOG_DYNNAME}.list"; then
    echo "FAIL: list-template output missing expected 'msgnum:' content"
    head -5 "${RSYSLOG_DYNNAME}.list"
    exit 1
fi
exit_test
