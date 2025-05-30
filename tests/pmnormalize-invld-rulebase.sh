#!/bin/bash
# add 2016-12-08 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/pmnormalize/.libs/pmnormalize")
parser(name="custom.pmnormalize" type="pmnormalize" rulebase="DOES-NOT-EXIST")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "cannot open rulebase .*DOES-NOT-EXIST"

exit_test
