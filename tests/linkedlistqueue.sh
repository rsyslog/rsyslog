#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Test for Linkedlist queue mode
# added 2009-05-20 by rgerhards
# Developed by Adiscon GmbH for the rsyslog testbench.
# Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
#
# Wait for the output line count before stopping the queue, then verify
# all 40,000 injected messages by sequence.
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
