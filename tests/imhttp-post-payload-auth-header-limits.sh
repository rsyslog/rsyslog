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

LONG_AUTH_VALUE="$(python3 - <<'PY'
print("A" * 20000)
PY
)"

assert_rejected() {
  local header_name="$1"
  local header_value="$2"
  local ret

  ret=$(curl -s -o /dev/null -w '%{http_code}' \
    -H "$header_name: $header_value" \
    -H Content-Type:application/json \
    http://localhost:$IMHTTP_PORT/postrequest \
    -d '[{"msg":"rejected"}]')

  if [ "$ret" = "200" ]; then
    echo "ERROR: oversized $header_name unexpectedly succeeded"
    error_exit 1
  fi
}

assert_rejected "Authorization" "ApiKey $LONG_AUTH_VALUE"
assert_rejected "Authorization" "Basic $LONG_AUTH_VALUE"
assert_rejected "X-API-Key" "$LONG_AUTH_VALUE"

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  -H 'X-API-Key:    secret-token-1' \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"msg":"whitespace-key"}]')
if [ "$ret" != "200" ]; then
  echo "ERROR: expected 200 for whitespace-padded X-API-Key, got $ret"
  error_exit 1
fi

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  --user user1:1234 \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"msg":"basic-ok"}]')
if [ "$ret" != "200" ]; then
  echo "ERROR: expected 200 for valid basic auth after oversized headers, got $ret"
  error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
content_count_check '[{"msg":"rejected"}]' 0
content_count_check '[{"msg":"whitespace-key"}]' 1
content_count_check '[{"msg":"basic-ok"}]' 1
exit_test
