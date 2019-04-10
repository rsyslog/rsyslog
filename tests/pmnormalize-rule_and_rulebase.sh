#!/bin/bash
# add 2019-04-10 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/pmnormalize/.libs/pmnormalize")
parser(name="custom.pmnormalize" type="pmnormalize" rule="" rulebase="")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "pmnormalize:.*you need to specify one of"

exit_test
