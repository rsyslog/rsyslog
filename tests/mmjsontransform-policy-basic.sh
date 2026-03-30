#!/bin/bash
# Validate mmjsontransform YAML policy rename/drop preprocessing, mode, and HUP reload.
. ${srcdir:=.}/diag.sh init

policy_file="$RSYSLOG_DYNNAME.policy.yaml"
cat > "$policy_file" <<'YAML'
version: 1
mode: flatten
map:
  rename:
    "usr": "user.name"
    "ctx.old": "ctx.new"
  drop:
    - "debug"
YAML

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmjsontransform/.libs/mmjsontransform")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!output%\n")

local4.* {
        set $.ret = parse_json($msg, "\$!input");
        action(type="mmjsontransform" policy="'$policy_file'" input="$!input" output="$!output")
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

startup
tcpflood -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"alice\", \"debug\": true, \"ctx\": { \"old\": 1 } }"'

# Ensure mtime change is visible on filesystems with coarse timestamp granularity.
sleep 2
cat > "$policy_file" <<'YAML'
version: 1
mode: unflatten
map:
  rename:
    "usr": "actor.name"
    "ctx.old": "ctx.after"
YAML

issue_HUP
tcpflood -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"bob\", \"debug\": true, \"ctx\": { \"old\": 2 } }"'

sleep 2
cat > "$policy_file" <<'YAML'
version: 1
mode: sideways
map:
  rename:
    "usr": "broken.name"
YAML

issue_HUP
tcpflood -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"carol\", \"debug\": true, \"ctx\": { \"old\": 3 } }"'

shutdown_when_empty
wait_shutdown

if [ ! -f "$RSYSLOG_OUT_LOG" ]; then
    echo "FAIL: expected output file '$RSYSLOG_OUT_LOG' was not created"
    error_exit 1
fi

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lines = [line.strip() for line in fh if line.strip()]

if len(lines) != 3:
    print("expected 3 JSON lines, got", len(lines))
    sys.exit(1)

first = json.loads(lines[0])
second = json.loads(lines[1])
third = json.loads(lines[2])

expected_first = {
    "user.name": "alice",
    "ctx.new": 1,
}
expected_second = {
    "actor": {"name": "bob"},
    "ctx": {"after": 2},
    "debug": True,
}
expected_third = {
    "actor": {"name": "carol"},
    "ctx": {"after": 3},
    "debug": True,
}

if first != expected_first:
    print("unexpected first output:", first)
    sys.exit(1)
if second != expected_second:
    print("unexpected second output:", second)
    sys.exit(1)
if third != expected_third:
    print("unexpected third output:", third)
    sys.exit(1)
PY
if [ $? -ne 0 ]; then
    error_exit 1
fi

if ! grep -q "failed to reload policy file" "$RSYSLOG_DYNNAME.started"; then
    echo "FAIL: expected reload failure to be logged for invalid policy mode"
    error_exit 1
fi

rm -f "$policy_file"
exit_test
