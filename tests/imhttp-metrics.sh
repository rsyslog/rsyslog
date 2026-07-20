#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify the imhttp metrics endpoint while the HTTP listener uses an
# OS-assigned port. The HTTP status and metrics response are the oracle.

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'"
       metricsPath="/metrics"
)
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"
curl -s http://localhost:$IMHTTP_PORT/metrics > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_check "imhttp_up 1"
content_check 'U__action_2D_0_2D_builtin:omfile__processed__total'
assert_content_missing 'action-0-builtin:omfile_processed_total'
exit_test