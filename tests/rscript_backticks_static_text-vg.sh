#!/bin/bash
## Validate that backticks with `echo` expand environment variables even
## when punctuation is directly attached to the name.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
if `echo Prefix-$MYVAR!` == "Prefix-42!" then
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
export MYVAR=42
startup_vg
injectmsg 0 1
shutdown_when_empty
wait_shutdown_vg
seq_check 0 0
exit_test

