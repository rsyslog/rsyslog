#!/bin/bash
# Test for Linkedlist queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under GPLv3
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=40000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
# set spool locations and switch queue to disk-only mode
$MainMsgQueueType LinkedList

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
