#!/bin/bash
## Validate ${VAR} expansion in backticks with static text.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
if `echo foo${MYVAR}bar` == "foo42bar" then
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
export MYVAR=42
startup_vg
injectmsg 0 1
shutdown_when_empty
wait_shutdown_vg
seq_check 0 0
exit_test
