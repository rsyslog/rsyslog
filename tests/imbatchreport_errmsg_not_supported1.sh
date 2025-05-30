#!/bin/bash
# add 2019-02-26 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/imbatchreport/.libs/imbatchreport")
input(type="imbatchreport" tag="t" ruleset="r" delete=".done$ .r" rename=".done$ .s .r" reports="*.*")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
shutdown_when_empty
wait_shutdown
content_check "'rename' and 'delete' are exclusive"

exit_test
