#!/bin/bash
# add 2018-08-02 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100 # we only check output fmt, so few messages are OK
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="builtin:omfile" template="outfmt")

:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
