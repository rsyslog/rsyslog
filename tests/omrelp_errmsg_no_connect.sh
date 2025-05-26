#!/bin/bash
# add 2018-11-14 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
# note: we "abuse" TCPFLOOD_PORT a bit as we already have it assigned
# the core fact is that nobody is listening on it.
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")
action(type="omrelp" target="127.0.0.1" port="'$TCPFLOOD_PORT'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown

content_check "could not connect to remote server"
exit_test
