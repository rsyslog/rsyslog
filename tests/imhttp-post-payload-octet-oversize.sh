#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
check_command_available python3
check_command_available timeout
generate_conf
IMHTTP_PORT="$(get_free_port)"
BODY=${RSYSLOG_DYNNAME}.octet-body
add_conf '
template(name="outfmt" type="string" string="%rawmsg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest"
      ruleset="ruleset"
      supportoctetcountedframing="on")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

python3 - "$BODY" <<'PY'
import sys

payload = b"A" * 17000
with open(sys.argv[1], "wb") as f:
    f.write(str(len(payload)).encode("ascii") + b" " + payload)
PY

ret=$(timeout 10 curl -s -o /dev/null --write-out '%{http_code}' \
  --data-binary @"$BODY" http://localhost:$IMHTTP_PORT/postrequest)
echo "oversize response: $ret"
if [ "$ret" != "200" ]; then
	echo "ERROR: oversized octet-counted request did not complete successfully"
	error_exit 1
fi

ret=$(timeout 10 curl -s -o /dev/null --write-out '%{http_code}' \
  --data-binary '5 hello' http://localhost:$IMHTTP_PORT/postrequest)
echo "follow-up response: $ret"
if [ "$ret" != "200" ]; then
	echo "ERROR: parser did not recover after oversized octet-counted frame"
	error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'hello'
exit_test
