#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

ret=$(printf 'not-a-gzip-stream' | curl -s -o /dev/null --write-out '%{http_code}' \
  --data-binary @- -H "Content-Encoding: gzip" http://localhost:$IMHTTP_PORT/postrequest)

echo "response: $ret"
if [ "$ret" = "200" ]; then
	echo "ERROR: malformed gzip payload was accepted"
	error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
if [ -f "$RSYSLOG_OUT_LOG" ]; then
	content_count_check 'not-a-gzip-stream' 0
fi
exit_test
