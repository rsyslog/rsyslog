#!/bin/bash
# added 2025-08-28 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# prepare invalid state file
echo '{ invalid json' > ${RSYSLOG_DYNNAME}.sendertrack

generate_conf
add_conf '
module(load="../plugins/omsendertrack/.libs/omsendertrack")
action(type="omsendertrack" statefile="'$RSYSLOG_DYNNAME'.sendertrack")
'
startup
shutdown_when_empty
wait_shutdown
content_check "error processing input json" 
exit_test

