#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'"
)
'
startup
curl -s http://localhost:$IMHTTP_PORT/healthz > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_check "OK"
exit_test
