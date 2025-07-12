#!/bin/bash
# added 2025-08-28 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omsendertrack/.libs/omsendertrack")
action(type="omsendertrack" statefile="'$RSYSLOG_DYNNAME'.sendertrack" bogus="1")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "parameter 'bogus' not known" 
exit_test

