#!/bin/bash
# The sole point of this test is that omusrmsg does not abort.
# We cannot check the actual outcome, as we would need to be running
# under root to do this. This test has explicitly been added to ensure
# we can do some basic testing even when not running as root. Additional
# tests may be added for the root case.
# addd 2018-08-05 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" :omusrmsg:nouser;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
# no check, if we still run at this point, the test is happy
exit_test
