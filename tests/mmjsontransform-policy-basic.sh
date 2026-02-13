#!/bin/bash
# Validate mmjsontransform YAML policy rename/drop preprocessing and HUP reload.
. ${srcdir:=.}/diag.sh init

policy_file="$RSYSLOG_DYNNAME.policy.yaml"
cat > "$policy_file" <<'YAML'
version: 1
map:
  rename:
    "usr": "user.name"
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
tcpflood -m1 -M '"{ \"usr\": \"alice\", \"debug\": true }"'

# Ensure mtime change is visible on filesystems with 1s timestamp granularity.
sleep 1
cat > "$policy_file" <<'YAML'
version: 1
map:
  rename:
    "usr": "actor.name"
YAML

issue_HUP
tcpflood -m1 -M '"{ \"usr\": \"bob\", \"debug\": true }"'

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

first = json.loads(lines[0])
second = json.loads(lines[1])

expected_first = {
    "user": {"name": "alice"}
}
expected_second = {
    "actor": {"name": "bob"},
    "debug": True,
}

if first != expected_first:
    print("unexpected first output:", first)
    sys.exit(1)
if second != expected_second:
    print("unexpected second output:", second)
    sys.exit(1)
PY

rm -f "$policy_file"
exit_test
