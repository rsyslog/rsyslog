#!/bin/bash
# added 2026-05-21 by AI agent; Released under ASL 2.0
# Verifies that jsonf property rendering handles a counted message value that
# ends with a backslash. The oracle is successful JSON parsing plus the exact
# trailing character, which catches regressions in the escape lookahead path.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="jsonf-backslash" type="list" option.jsonftree="on") {
         property(outname="message" name="msg" format="jsonf")
}

local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="jsonf-backslash")
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z hostname1 sender - tag [tcpflood@32473 MSGNUM="0"] endslash\'
shutdown_when_empty
wait_shutdown

python3 - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    actual = json.loads(fh.read())

expected = "endslash\\"
message = actual.get("message", "")
if message.strip() != expected:
    print("invalid response generated")
    print("actual message: %r" % message)
    print("expected message after strip: %r" % expected)
    sys.exit(1)
PY
exit_test
