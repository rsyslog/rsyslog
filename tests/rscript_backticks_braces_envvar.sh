#!/bin/bash
## Validate ${VAR} expansion in backticks with static text.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list") {
    property(name="msg" field.delimiter="58" field.number="2")
    constant(value="\n")
}

if `echo foo${MYVAR}bar` == "foo42bar" and $msg contains "msgnum" then
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
export MYVAR=42
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
seq_check 0 0
exit_test
