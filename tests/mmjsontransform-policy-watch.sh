#!/bin/bash
# Validate mmjsontransform watch-based YAML policy reload, including
# rename-over-save updates.

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

# port is assigned by diag.sh from listenPortFileName
export POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
export POLICY_TMP="$(pwd)/${RSYSLOG_DYNNAME}.policy.tmp.yaml"

cat > "$POLICY_FILE" <<'YAML'
version: 1
mode: flatten
map:
  rename:
    "usr": "user.name"
    "ctx.old": "ctx.new"
YAML

generate_conf
add_conf '
global(processInternalMessages="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmjsontransform/.libs/mmjsontransform")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!output%\n")

local4.* {
        set $.ret = parse_json($msg, "\$!input");
        action(type="mmjsontransform" policy="'$POLICY_FILE'" policyWatch="on" policyWatchDebounce="200ms" input="$!input" output="$!output")
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

check_json() {
python3 - "$1" "$2" <<'PY'
import json
import sys

path = sys.argv[1]
expected = json.loads(sys.argv[2])

with open(path, "r", encoding="utf-8") as fh:
    lines = [line.strip() for line in fh if line.strip()]

if len(lines) != 1:
    print("expected 1 JSON line, got", len(lines))
    sys.exit(1)

obj = json.loads(lines[0])
if obj != expected:
    print("unexpected output:", obj)
    sys.exit(1)
PY
}

startup

./msleep 1000

tcpflood -Ttcp -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"alice\", \"ctx\": { \"old\": 1 } }"'
wait_file_lines "$RSYSLOG_OUT_LOG" 1 100
check_json "$RSYSLOG_OUT_LOG" '{"user.name":"alice","ctx.new":1}'
if [ $? -ne 0 ]; then error_exit 1; fi
: > "$RSYSLOG_OUT_LOG"

cat > "$POLICY_FILE" <<'YAML'
version: 1
mode: unflatten
map:
  rename:
    "usr": "actor.name"
    "ctx.old": "ctx.after"
YAML
./msleep 1000

tcpflood -Ttcp -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"bob\", \"ctx\": { \"old\": 2 }, \"debug\": true }"'
wait_file_lines "$RSYSLOG_OUT_LOG" 1 100
check_json "$RSYSLOG_OUT_LOG" '{"actor":{"name":"bob"},"ctx":{"after":2},"debug":true}'
if [ $? -ne 0 ]; then error_exit 1; fi
: > "$RSYSLOG_OUT_LOG"

cat > "$POLICY_TMP" <<'YAML'
version: 1
mode: flatten
map:
  rename:
    "usr": "user.name"
    "ctx.old": "ctx.new"
YAML
mv -f "$POLICY_TMP" "$POLICY_FILE"
./msleep 1000

tcpflood -Ttcp -m1 -M '"<166>Mar 10 01:00:00 host app: { \"usr\": \"carol\", \"ctx\": { \"old\": 3 } }"'
wait_file_lines "$RSYSLOG_OUT_LOG" 1 100
check_json "$RSYSLOG_OUT_LOG" '{"user.name":"carol","ctx.new":3}'
if [ $? -ne 0 ]; then error_exit 1; fi

shutdown_when_empty
wait_shutdown

rm -f "$POLICY_FILE" "$POLICY_TMP"
exit_test
