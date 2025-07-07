#!/bin/bash
# Test that '$' in double-quoted string constants raise a meaningful
# error message and do not cause rsyslog to segfault.
# added 2019-12-30 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'") # order is important ...
set $!test="test$" # ... as after this syntax error nothing is really taken up
'
startup
shutdown_when_empty
wait_shutdown
content_check '$-sign in double quotes must be escaped'
exit_test
