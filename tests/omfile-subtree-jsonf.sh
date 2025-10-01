#!/bin/bash
# Validate that subtree templates provide data to jsonf list templates
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="eventSubtree" type="subtree" subtree="$!event")
template(name="jsonfList" type="list" option.jsonf="on") {
        property(outname="message" name="$.payload" format="jsonf")
}

if $msg contains "msgnum:" then {
        set $!event!level = "error";
        set $!event!code = 500;
        set $.payload = exec_template("eventSubtree");
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="jsonfList")
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], 'r', encoding='utf-8') as fh:
    payload = json.load(fh)

expected_message = '{ "level": "error", "code": 500 }'
if payload.get("message") != expected_message:
    print('invalid JSON generated')
    print('################# actual JSON is:')
    print(json.dumps(payload, indent=2, sort_keys=True))
    print('################# expected JSON was:')
    print(json.dumps({"message": expected_message}, indent=2, sort_keys=True))
    sys.exit(1)
PY

exit_test

