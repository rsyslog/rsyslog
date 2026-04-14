#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'"
       healthCheckApiKeyFile="'$srcdir'/testsuites/imhttp-apikeys.txt")
'
startup

ret=$(curl -s -o /dev/null -w '%{http_code}' http://localhost:$IMHTTP_PORT/healthz)
if [ "$ret" != "401" ]; then
  echo "ERROR: expected 401 without API key, got $ret"
  error_exit 1
fi

curl -s -H 'Authorization: ApiKey secret-token-1' \
  http://localhost:$IMHTTP_PORT/healthz > "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
content_check "OK"
exit_test
