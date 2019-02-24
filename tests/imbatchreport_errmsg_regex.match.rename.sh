#!/bin/bash
# add 2019-02-26 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../contrib/imbatchreport/.libs/imbatchreport")

input(type="imbatchreport" tag="t" rename=".done$ .s.done .r" reports="./*.done")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check "Normal renaming leaves files in glob scope: Instance ignored to avoid loops."

exit_test
