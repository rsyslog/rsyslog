#!/bin/bash
# Validate mmjsontransform restructures dotted JSON keys into nested containers.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmjsontransform/.libs/mmjsontransform")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!output%\n%$!flattened%\n")

local4.* {
        set $.ret = parse_json("{ \"a.b\": \"value\", \"a\": { \"existing\": 1 }, \"simple\": \"text\", \"nested.level.deep\": true, \"arr\": [ { \"inner.key\": 1 }, { \"plain\": 2 } ] }", "\$!input");
        action(type="mmjsontransform" input="$!input" output="$!output")
        action(type="mmjsontransform" mode="flatten" input="$!output" output="$!flattened")
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lines = [line.strip() for line in fh if line.strip()]

if len(lines) != 2:
    print("expected 2 JSON lines, got", len(lines))
    sys.exit(1)

unflattened = json.loads(lines[0])
flattened = json.loads(lines[1])

expected_unflattened = {
    "a": {
        "b": "value",
        "existing": 1,
    },
    "simple": "text",
    "nested": {"level": {"deep": True}},
    "arr": [
        {"inner": {"key": 1}},
        {"plain": 2},
    ],
}

expected_flattened = {
    "a.b": "value",
    "a.existing": 1,
    "simple": "text",
    "nested.level.deep": True,
    "arr": [
        {"inner.key": 1},
        {"plain": 2},
    ],
}

if unflattened != expected_unflattened:
    print("unexpected unflattened output:", unflattened)
    sys.exit(1)

if flattened != expected_flattened:
    print("unexpected flattened output:", flattened)
    sys.exit(1)
PY

exit_test
