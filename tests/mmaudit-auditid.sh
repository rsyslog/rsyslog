#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Preserve every character between audit( and ): in the structured audit ID.
# Literal RFC5424 MSG inputs cover a normal record, leading whitespace, and a
# non-audit control. Check parse success, integer type, exact key/value data,
# original message preservation, and non-audit rejection before checking IDs,
# so a setup/parser failure cannot masquerade as the one-character regression.
# Wait for three output records and inspect semantic JSON after proper shutdown;
# record order is irrelevant and no fixed startup delay is used. Use the
# configured Python interpreter with Python 2.7/3-compatible oracle syntax.
. ${srcdir:=.}/diag.sh init
require_plugin mmaudit
export NUMMESSAGES=3
generate_conf
if [ "${MMAUDIT_ID_YAML:-}" = YES ]; then
    cat > "${RSYSLOG_DYNNAME}.yaml" <<'YAML_EOF'
version: 2
modules:
  - load: ../plugins/mmaudit/.libs/mmaudit
templates:
  - name: auditout
    type: subtree
    subtree: "$!"
rulesets:
  - name: auditparse
    script: |
      if $programname == "audit-test" then {
        :mmaudit:
        set $!test!parse = $parsesuccess;
        set $!test!message = $msg;

YAML_EOF
    printf '        action(type="omfile" file="%s" template="auditout")\n      }\n' "$RSYSLOG_OUT_LOG" >> "${RSYSLOG_DYNNAME}.yaml"
    add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")
call auditparse'
else
    add_conf '
module(load="../plugins/mmaudit/.libs/mmaudit")
template(name="auditout" type="subtree" subtree="$!")
if $programname == "audit-test" then {
:mmaudit:
set $!test!parse = $parsesuccess;
set $!test!message = $msg;
action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="auditout")
}
'
fi
if ! ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
    > "${RSYSLOG_DYNNAME}.config-check.log" 2>&1; then
    cat "${RSYSLOG_DYNNAME}.config-check.log"
    error_exit 1
fi
startup
printf '%s\n' \
    '<134>1 2026-01-01T00:00:00Z localhost audit-test - - - type=1400 audit(1700000000.123:42): key=value pid=42' \
    '<134>1 2026-01-01T00:00:00Z localhost audit-test - - -    type=1300 audit(2800000000.456:73): key=other pid=7' \
    '<134>1 2026-01-01T00:00:00Z localhost audit-test - - - ordinary non-audit record' > "${RSYSLOG_DYNNAME}.input"
injectmsg_file "${RSYSLOG_DYNNAME}.input"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
$PYTHON - "$RSYSLOG_OUT_LOG" <<'PY'
from __future__ import print_function

import io
import json
import sys

with io.open(sys.argv[1], encoding="utf-8") as stream:
    records = [json.loads(line) for line in stream]
assert len(records) == 3, records
by_message = {record["test"]["message"]: record for record in records}
expected = [
    ("type=1400 audit(1700000000.123:42): key=value pid=42", 1400, "1700000000.123:42", {"key": "value", "pid": "42"}),
    ("   type=1300 audit(2800000000.456:73): key=other pid=7", 1300, "2800000000.456:73", {"key": "other", "pid": "7"}),
]
assert set(by_message) == set([item[0] for item in expected] + ["ordinary non-audit record"]), by_message
control = by_message["ordinary non-audit record"]
assert control["test"]["parse"] == "FAIL" and "audit" not in control, control
print("non-audit and original-message controls: PASS")
for message, audit_type, audit_id, data in expected:
    record = by_message[message]
    assert record["test"]["parse"] == "OK", record
    assert record["audit"]["hdr"]["type"] == audit_type, record
    assert record["audit"]["data"] == data, record
print("parse-success, type, key/value, and leading-whitespace controls: PASS")
sys.stdout.flush()
for message, audit_type, audit_id, data in expected:
    actual = by_message[message]["audit"]["hdr"]["auditid"]
    assert actual == audit_id, (actual, audit_id)
print("complete audit identifiers: PASS")
PY
if [ "$?" -ne 0 ]; then
    error_exit 1
fi
exit_test
