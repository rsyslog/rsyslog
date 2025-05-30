#!/bin/bash
# Test for discard functionality
# This test checks if discard works. It is not a perfect test but
# will find at least segfaults and obviously not discarded messages.
# added 2009-07-30 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
# uncomment for debugging support:
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
generate_conf
add_conf '
:msg, contains, "00000000" ~

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check 1 $((NUMMESSAGES-1))
exit_test
