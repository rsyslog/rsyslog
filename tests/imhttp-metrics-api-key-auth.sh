#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify the imhttp metrics endpoint while the HTTP listener uses an
# OS-assigned port. The HTTP status and metrics response are the oracle.

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'"
       metricsPath="/metrics"
       metricsApiKeyFile="'$srcdir'/testsuites/imhttp-apikeys.txt")
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"

ret=$(curl -s -o /dev/null -w '%{http_code}' http://localhost:$IMHTTP_PORT/metrics)
if [ "$ret" != "401" ]; then
  echo "ERROR: expected 401 without API key, got $ret"
  error_exit 1
fi

curl -s -H 'Authorization: ApiKey secret-token-2' \
  http://localhost:$IMHTTP_PORT/metrics > "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
content_check "imhttp_up 1"
exit_test
