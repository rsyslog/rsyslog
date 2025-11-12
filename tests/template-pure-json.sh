#!/bin/bash
# added 2018-02-10 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list" option.jsonftree="on") {
	 property(outname="message" name="msg" format="jsonf")
	 constant(outname="@version" value="1" format="jsonf")
}

local4.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
export EXPECTED_JSON='{"message":" msgnum:00000000:", "@version": "1"}'
python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import os
import sys

expected = json.loads(os.environ['EXPECTED_JSON'])
with open(sys.argv[1], 'r', encoding='utf-8') as fh:
    actual = json.loads(fh.read())

if actual != expected:
    print('invalid response generated')
    print('################# actual JSON is:')
    print(json.dumps(actual, indent=2, sort_keys=True))
    print('################# expected JSON was:')
    print(json.dumps(expected, indent=2, sort_keys=True))
    sys.exit(1)
PY
exit_test
