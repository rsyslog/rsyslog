#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# An explicitly configured backslash must separate extension fields, as the
# documented delimiter parameter promises. The pipe-delimiter control preserves
# header escapes and escaped delimiters, backslashes, and equals signs for
# ordinary delimiters. A single-field backslash control retains recognized
# escapes; leading/trailing separators and empty values are also covered.
# The YAML wrapper exercises the same action parameter.
# Inject literal bytes through imdiag, wait for all four JSON records, and
# check semantic fields after proper shutdown; no timing assumption or socket
# helper is needed. Assert the controls before the backslash regression.
. ${srcdir:=.}/diag.sh init
require_plugin mmleefparse
export NUMMESSAGES=4

generate_conf
if [ "${MMLEEF_DELIMITER_YAML:-}" = YES ]; then
	cat > "${RSYSLOG_DYNNAME}.yaml" <<'YAML_EOF'
version: 2
modules:
  - load: ../plugins/mmleefparse/.libs/mmleefparse
templates:
  - name: leefout
    type: subtree
    subtree: "$!leef"
rulesets:
  - name: leefparse
    statements:
      - if: '$programname == "leefbackslash"'
        then:
          - type: mmleefparse
            container: "!leef"
            delimiter: '\\'
          - type: omfile
            file: "@OUTPUT@"
            template: leefout
      - if: '$programname == "leefpipe"'
        then:
          - type: mmleefparse
            container: "!leef"
            delimiter: '|'
          - type: omfile
            file: "@OUTPUT@"
            template: leefout
YAML_EOF
	# Stream through an owned temporary file: BSD sed requires a different -i syntax.
	sed "s|@OUTPUT@|$RSYSLOG_OUT_LOG|g" "${RSYSLOG_DYNNAME}.yaml" > "${RSYSLOG_DYNNAME}.yaml.tmp" ||
		error_exit 1 "cannot substitute YAML output path"
	mv "${RSYSLOG_DYNNAME}.yaml.tmp" "${RSYSLOG_DYNNAME}.yaml" ||
		error_exit 1 "cannot install YAML configuration"
	add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")
call leefparse'
else
	add_conf '
module(load="../plugins/mmleefparse/.libs/mmleefparse")
template(name="leefout" type="subtree" subtree="$!leef")
if $programname == "leefbackslash" then {
    action(type="mmleefparse" container="!leef" delimiter="\\\\")
    action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="leefout")
}
if $programname == "leefpipe" then {
    action(type="mmleefparse" container="!leef" delimiter="|")
    action(type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="leefout")
}

'
fi
startup
printf '%s\n' '<134>Jan  1 00:00:00 localhost leefbackslash: LEEF:1.0|Ven\|dor|Product|1|backslash|k1=v1\k2=v2' \
	'<134>Jan  1 00:00:00 localhost leefbackslash: LEEF:1.0|Ven\|dor|Product|1|escapes|k\=ey=one\\two\|part\=end' \
	'<134>Jan  1 00:00:00 localhost leefbackslash: LEEF:1.0|Ven\|dor|Product|1|edges|\k1=v1\empty=\k2=v2\' \
	'<134>Jan  1 00:00:00 localhost leefpipe: LEEF:1.0|Ven\|dor|Product|1|pipe|k1=v1\|part|k2=v2|slash=one\\two|k\=ey=v\=al|empty=' > "${RSYSLOG_DYNNAME}.input"
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
assert len(records) == 4, records
by_event = {record["header"]["eventID"]: record for record in records}
assert set(by_event) == {"backslash", "pipe", "escapes", "edges"}, by_event
expected = {
    "pipe": {"k1": "v1|part", "k2": "v2", "slash": r"one\two", "k=ey": "v=al", "empty": ""},
    "escapes": {"k=ey": r"one\two|part=end"},
    "backslash": {"k1": "v1", "k2": "v2"},
    "edges": {"k1": "v1", "empty": "", "k2": "v2"},
}
for event in ("pipe", "escapes", "backslash", "edges"):
    record = by_event[event]
    assert record["header"]["vendor"] == "Ven|dor", record
    assert record["fields"] == expected[event], (event, record["fields"], expected[event])
    print("{} delimiter and escaping: PASS".format(event))
    sys.stdout.flush()
PY
if [ "$?" -ne 0 ]; then
	error_exit 1 "LEEF delimiter or escaping fields differ"
fi
exit_test
