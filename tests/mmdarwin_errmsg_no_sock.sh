#!/bin/bash
# add 2019-04-02 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/mmdarwin/.libs/mmdarwin")
action(type="mmdarwin" socketpath="/invalid" key="invalid" fields="invalid")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

 startup
shutdown_when_empty
wait_shutdown
content_check --regex "error connecting to Darwin.*/invalid"

exit_test
