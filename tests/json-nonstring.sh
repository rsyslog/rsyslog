#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released  under ASL 2.0
# written 2019-05-10 by Rainer Gerhards
# Verify direct jsonf dataType conversion without cbool(): empty and single zero
# render as false, while nonempty nonzero strings remain true. Explicit onEmpty
# keep/null/skip policies are independent controls. One injected message produces
# one JSON object; synchronized shutdown and parsed values/types form the oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="json" type="list" option.jsonf="on") {
	property(outname="number_0" format="jsonf" name="$!val0" datatype="number")
	property(outname="bool_0" format="jsonf" name="$!val0" datatype="bool")

	property(outname="empty" format="jsonf" name="$!empty" datatype="auto")
	property(outname="empty_skip" format="jsonf" name="$!empty" onEmpty="skip")
	property(outname="empty_null" format="jsonf" name="$!empty" onEmpty="null")
	property(outname="empty_number" format="jsonf" name="$!empty" datatype="number")
	property(outname="empty_bool" format="jsonf" name="$!empty" datatype="bool")
	property(outname="empty_bool_keep" format="jsonf" name="$!empty" datatype="bool" onEmpty="keep")
	property(outname="empty_bool_null" format="jsonf" name="$!empty" datatype="bool" onEmpty="null")
	property(outname="empty_bool_skip" format="jsonf" name="$!empty" datatype="bool" onEmpty="skip")
	property(outname="bool_false_string" format="jsonf" name="$!false_string" datatype="bool")
	property(outname="bool_double_zero" format="jsonf" name="$!double_zero" datatype="bool")

	property(outname="auto_string" format="jsonf" name="$!string" datatype="auto")

	property(outname="auto" format="jsonf" name="$!val" datatype="auto" onEmpty="null")
	property(outname="number" format="jsonf" name="$!val" datatype="number")
	property(outname="bool" format="jsonf" name="$!val" datatype="bool")
	property(outname="string" format="jsonf" name="$!val" datatype="string")
	property(outname="no_datatype" format="jsonf" name="$!val")
}

set $!val0 = 0;
set $!val = 42;
set $!empty = "";
set $!string = "1.2.3.4";
set $!false_string = "false";
set $!double_zero = "00";
if $msg contains "msgnum:" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="json")
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
"$PYTHON" - "$RSYSLOG_OUT_LOG" <<'PYJSON'
from __future__ import print_function, unicode_literals

import io
import json
import sys

with io.open(sys.argv[1], encoding="utf-8") as output:
    records = [json.loads(line) for line in output]
expected = {
    "number_0": 0, "bool_0": False, "empty": "", "empty_null": None,
    "empty_number": 0, "empty_bool": False, "empty_bool_keep": False,
    "empty_bool_null": None, "bool_false_string": True, "bool_double_zero": True,
    "auto_string": "1.2.3.4", "auto": 42, "number": 42, "bool": True,
    "string": "42", "no_datatype": "42",
}
if records != [expected]:
    raise SystemExit("unexpected JSON values: {!r}; expected {!r}".format(records, [expected]))
for field, value in expected.items():
    if type(records[0][field]) is not type(value):
        raise SystemExit("unexpected JSON type for " + field)
PYJSON
[ "$?" -eq 0 ] || error_exit 1 "JSON value/type oracle failed"
exit_test
