#!/bin/bash
# This test checks if omfile segfaults when a file open() in dynacache mode fails.
# The test is mimiced after a real-life scenario (which, of course, was much more
# complex).
#
# added 2010-03-09 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$template outfmt,"%msg:F,58:3%\n"
$template dynfile,"%msg:F,58:2%.log" # complete name is in message
$OMFileFlushOnTXEnd on
$OMFileAsyncWriting off
$DynaFileCacheSize 4
local0.* ?dynfile;outfmt
'
startup
# Send the handcrafted messages in one imdiag session so the invalid open
# and first following valid dynafile write can be processed in one batch.
cat > "$RSYSLOG_DYNNAME.input" <<EOF
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.0.log:0
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.1.log:1
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.2.log:2
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.3.log:3
<129>Mar 10 01:00:00 172.20.245.8 tag msg:/proc/rsyslog.error.file:boom
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.0.log:4
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.1.log:5
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.2.log:6
<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.3.log:7
EOF
injectmsg_file "$RSYSLOG_DYNNAME.input"
# done message generation
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
cat $RSYSLOG_DYNNAME.out.*.log > $RSYSLOG_OUT_LOG
seq_check 0 7
exit_test
