#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify imhttp static file serving while the HTTP listener uses an
# OS-assigned port. The fetched file content is the oracle.

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="0"
       listenPortFileName="'$IMHTTP_PORT_FILE'"
			 documentroot="'$srcdir'/testsuites/docroot")
'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"
curl -s http://localhost:$IMHTTP_PORT/file.txt > "$RSYSLOG_OUT_LOG"
shutdown_when_empty
echo "file name: $RSYSLOG_OUT_LOG"
content_check "This is a test of get page"
exit_test
