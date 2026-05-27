#!/bin/bash
# Verify jsonftree conflict fallback still emits valid flat JSON (ASL 2.0).
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="nested_fallback" type="list" option.jsonftree="on") {
         constant(outname="a" value="A" format="jsonf")
         constant(outname="a.b" value="B" format="jsonf")
}

local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="nested_fallback")
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], 'r', encoding='utf-8') as fh:
    actual = json.loads(fh.read())

expected = {"a": "A", "a.b": "B"}
if actual != expected:
    print('invalid fallback JSON generated')
    print('actual:', actual)
    print('expected:', expected)
    sys.exit(1)
PY
exit_test
