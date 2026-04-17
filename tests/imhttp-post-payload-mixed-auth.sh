#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest"
      ruleset="ruleset"
      basicAuthFile="'$srcdir'/testsuites/htpasswd"
      apiKeyFile="'$srcdir'/testsuites/imhttp-apikeys.txt")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"msg":"unauthorized"}]')
if [ "$ret" != "401" ]; then
  echo "ERROR: expected 401 without auth, got $ret"
  error_exit 1
fi

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  --user user1:1234 \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"msg":"basic"}]')
if [ "$ret" != "200" ]; then
  echo "ERROR: expected 200 for valid basic auth, got $ret"
  error_exit 1
fi

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  -H 'Authorization: ApiKey secret-token-2' \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"msg":"apikey"}]')
if [ "$ret" != "200" ]; then
  echo "ERROR: expected 200 for valid API key, got $ret"
  error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
content_count_check '[{"msg":"basic"}]' 1
content_count_check '[{"msg":"apikey"}]' 1
exit_test
