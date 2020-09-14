#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'"
			 documentroot="'$srcdir'/testsuites/docroot")
'
startup
curl -s http://localhost:$IMHTTP_PORT/file.txt > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
echo "file name: $RSYSLOG_OUT_LOG"
content_check "This is a test of get page"
exit_test
