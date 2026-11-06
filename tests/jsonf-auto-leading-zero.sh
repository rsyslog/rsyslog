#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# JSONF auto must convert digit-only scalar strings into valid JSON numbers,
# including leading zeros and values beyond native integer ranges. Parsed JSON
# checks exact arbitrary-precision values/types; string and ordinary-number
# controls distinguish conversion from blanket quoting. One injected message
# and queue-empty shutdown make the single-record oracle independent of timing.
. ${srcdir:=.}/diag.sh init
generate_conf
if [ "${JSONF_AUTO_YAML:-0}" = 1 ]; then
    add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")'
    cat > "${RSYSLOG_DYNNAME}.yaml" <<'YAMLEOF'
templates:
  - name: auto_numbers
    type: list
    option.jsonf: "on"
    elements:
      - property:
          name: "$.leading"
          outname: leading
          format: jsonf
          dataType: auto
      - property:
          name: "$.allzero"
          outname: allzero
          format: jsonf
          dataType: auto
      - property:
          name: "$.zero"
          outname: zero
          format: jsonf
          dataType: auto
      - property:
          name: "$.one"
          outname: one
          format: jsonf
          dataType: auto
      - property:
          name: "$.large"
          outname: large
          format: jsonf
          dataType: auto
      - property:
          name: "$.text"
          outname: text
          format: jsonf
          dataType: auto
      - property:
          name: "$.string"
          outname: string
          format: jsonf
          dataType: string
YAMLEOF
else
    add_conf '
template(name="auto_numbers" type="list" option.jsonf="on") {
    property(name="$.leading" outname="leading" format="jsonf" dataType="auto")
    property(name="$.allzero" outname="allzero" format="jsonf" dataType="auto")
    property(name="$.zero" outname="zero" format="jsonf" dataType="auto")
    property(name="$.one" outname="one" format="jsonf" dataType="auto")
    property(name="$.large" outname="large" format="jsonf" dataType="auto")
    property(name="$.text" outname="text" format="jsonf" dataType="auto")
    property(name="$.string" outname="string" format="jsonf" dataType="string")
}
'
fi
add_conf '
if $msg contains "msgnum:" then {
    set $.leading = "001";
    set $.allzero = "00";
    set $.zero = "0";
    set $.one = "1";
    set $.large = "00012345678901234567890123456789012345678901234567890";
    set $.text = "12x";
    set $.string = "001";
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="auto_numbers")
}
'
# Configuration validation separates fixture errors from the output regression.
../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" \
    > "${RSYSLOG_DYNNAME}.config-check.log" 2>&1
if [ $? -ne 0 ]; then
    cat "${RSYSLOG_DYNNAME}.config-check.log"
    error_exit 1 "JSONF auto fixture configuration is invalid"
fi
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
"$PYTHON" - "$RSYSLOG_OUT_LOG" <<'PYEOF'
from __future__ import print_function
import io
import json
import sys
with io.open(sys.argv[1], encoding='utf-8') as stream:
    records = stream.read().splitlines()
if len(records) != 1:
    raise SystemExit('expected one JSON record, got %d' % len(records))
try:
    actual = json.loads(records[0])
except ValueError as error:
    raise SystemExit('invalid JSONF auto output: %s; record=%s' % (error, records[0]))
expected = {'leading': 1, 'allzero': 0, 'zero': 0, 'one': 1,
            'large': 12345678901234567890123456789012345678901234567890,
            'text': '12x', 'string': '001'}
if actual != expected:
    raise SystemExit('unexpected values: %r' % actual)
try:
    integer_types = (int, long)
except NameError:
    integer_types = (int,)
for key in ('leading', 'allzero', 'zero', 'one', 'large'):
    if type(actual[key]) not in integer_types:
        raise SystemExit('expected exact integer for %s, got %r' % (key, actual[key]))
print('PASS: JSONF auto numbers preserve exact values and string controls')
PYEOF
if [ $? -ne 0 ]; then
    error_exit 1
fi
exit_test
