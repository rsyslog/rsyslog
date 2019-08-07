#!/bin/bash
# same as omusrmsg-noabort, but with legacy syntax.
# addd 2018-08-05 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omusrmsg" users="nouser" template="outfmt")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
# no check, if we still run at this point, the test is happy
exit_test
