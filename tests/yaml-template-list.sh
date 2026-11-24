#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Tests YAML list template support with property and constant elements,
# including modifiers:
#   - property: name, droplastlf, format (json/jsonf), datatype, onempty
#   - constant: value
#   - template-level option: option.jsonf
#
# A string template extracts sequence numbers, while a plain-text list template
# verifies property modifiers and a constant newline for every injected message.
#
# A second YAML-defined JSON template verifies direct boolean empty/default-keep
# and explicit keep/null/skip handling, with zero/nonzero and nonempty controls.
# After synchronized shutdown, every injected message must yield one parsed JSON
# object with exactly the expected values and types; no cbool() normalizes inputs.
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


  - name: boolfmt
    type: list
    option.jsonf: "on"
    elements:
      - property: {name: "$!empty", outname: empty_bool, datatype: bool, format: jsonf}
      - property: {name: "$!empty", outname: empty_keep, datatype: bool, format: jsonf, onempty: keep}
      - property: {name: "$!empty", outname: empty_null, datatype: bool, format: jsonf, onempty: null}
      - property: {name: "$!empty", outname: empty_skip, datatype: bool, format: jsonf, onempty: skip}
      - property: {name: "$!zero", outname: zero, datatype: bool, format: jsonf}
      - property: {name: "$!nonzero", outname: nonzero, datatype: bool, format: jsonf}
      - property: {name: "$!false_string", outname: false_string, datatype: bool, format: jsonf}
      - property: {name: "$!double_zero", outname: double_zero, datatype: bool, format: jsonf}

rulesets:
  - name: main
    statements:
      - if: '$msg contains "msgnum:"'
        then:
          - set: {var: "$!empty", expr: '""'}
          - set: {var: "$!zero", expr: "0"}
          - set: {var: "$!nonzero", expr: "42"}
          - set: {var: "$!false_string", expr: '"false"'}
          - set: {var: "$!double_zero", expr: '"00"'}
          - type: omfile
            template: boolfmt
            file: "${RSYSLOG_DYNNAME}.bool"
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
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
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
# Every line must contain the msgnum: marker — check that ALL lines match,
# not just some (property extraction must have worked for each message).
MATCH=$(grep -c "msgnum:" "${RSYSLOG_DYNNAME}.list")
if [ "$MATCH" -ne "$NUMMESSAGES" ]; then
    echo "FAIL: expected all $NUMMESSAGES lines to contain 'msgnum:', got $MATCH"
    head -5 "${RSYSLOG_DYNNAME}.list"
    exit 1
fi
"$PYTHON" - "${RSYSLOG_DYNNAME}.bool" "$NUMMESSAGES" <<'PYJSON'
from __future__ import print_function, unicode_literals

import io
import json
import sys

with io.open(sys.argv[1], encoding="utf-8") as output:
    records = [json.loads(line) for line in output]
expected = {
    "empty_bool": False, "empty_keep": False, "empty_null": None,
    "zero": False, "nonzero": True, "false_string": True, "double_zero": True,
}
if len(records) != int(sys.argv[2]):
    raise SystemExit("unexpected JSON record count: " + str(len(records)))
for record in records:
    if record != expected:
        raise SystemExit("unexpected JSON values: {!r}; expected {!r}".format(record, expected))
    for field, value in expected.items():
        if type(record[field]) is not type(value):
            raise SystemExit("unexpected JSON type for " + field)
PYJSON
[ "$?" -eq 0 ] || error_exit 1 "YAML JSON value/type oracle failed"
exit_test
