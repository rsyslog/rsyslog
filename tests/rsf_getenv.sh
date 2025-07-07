#!/bin/bash
# Test for the getenv() rainerscript function
# this is a quick test, but it guarantees that the code path is
# at least progressed (but we do not check for unset envvars!)
# added 2009-11-03 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
export NUMMESSAGES=10000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export MSGNUM="msgnum:"
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
if $msg contains getenv("MSGNUM") then ?dynfile;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
