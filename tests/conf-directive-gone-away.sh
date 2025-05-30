#!/bin/bash
# addd 2018-09-04 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$optimizeForUniprocessor
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
content_check "config directive is no longer supported"
exit_test
