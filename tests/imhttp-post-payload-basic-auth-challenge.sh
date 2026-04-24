#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
HEADERS=${RSYSLOG_DYNNAME}.headers
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest"
      ruleset="ruleset"
      basicauthfile="'$srcdir'/testsuites/htpasswd")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

ret=$(curl -s -D "$HEADERS" -o /dev/null --write-out '%{http_code}' \
  --user user1:badpass http://localhost:$IMHTTP_PORT/postrequest -d 'rejected')

echo "response: $ret"
if [ "$ret" != "401" ]; then
	echo "ERROR: invalid Basic auth credentials returned $ret instead of 401"
	error_exit 1
fi
if ! grep -qi '^WWW-Authenticate: Basic ' "$HEADERS"; then
	echo "ERROR: Basic auth challenge header missing"
	error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
if [ -f "$RSYSLOG_OUT_LOG" ]; then
	content_count_check 'rejected' 0
fi
exit_test
