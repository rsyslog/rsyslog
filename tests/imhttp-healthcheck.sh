#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify the imhttp healthcheck endpoint while the HTTP listener uses an
# OS-assigned port. The HTTP status or response body is the oracle.

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'"
)
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"
curl -s http://localhost:$IMHTTP_PORT/healthz > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_check "OK"
exit_test
