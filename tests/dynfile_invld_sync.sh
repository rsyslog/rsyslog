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
# we send handcrafted message. We have a dynafile cache of 4, and now send one message
# each to fill up the cache.
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.0.log:0"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.1.log:1"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.2.log:2"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.3.log:3"
# the next one has caused a segfault in practice
# note that /proc/rsyslog.error.file must not be creatable
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:/proc/rsyslog.error.file:boom"
# some more writes
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.0.log:4"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.1.log:5"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.2.log:6"
injectmsg_literal "<129>Mar 10 01:00:00 172.20.245.8 tag msg:$RSYSLOG_DYNNAME.out.3.log:7"
# done message generation
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
cat $RSYSLOG_DYNNAME.out.*.log > $RSYSLOG_OUT_LOG
seq_check 0 7
exit_test
