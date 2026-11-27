#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify a 1024-byte scrape limit rejects an oversized response with HTTP 500.
# The status returned by curl after listener readiness is the failure oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
global(prometheus.maxResponseSize="1024")
module(load="../contrib/imhttp/.libs/imhttp" ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"
ret=$(curl -s -o /dev/null -w '%{http_code}' http://localhost:$IMHTTP_PORT/metrics)
shutdown_when_empty
wait_shutdown
if [ "$ret" != "500" ]; then
  echo "expected HTTP 500 for response above limit, got $ret"
  error_exit 1
fi
exit_test
