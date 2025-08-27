#!/bin/bash
# same as omusrmsg-noabort, but with legacy syntax.
# add 2018-08-05 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omusrmsg" users="nouser" template="outfmt")
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
	}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
# while we cannot check if the messages arrived in user sessions, we
# can check that all messages arrive in the file, which we consider as
# an indication that everything went well.
seq_check
exit_test
