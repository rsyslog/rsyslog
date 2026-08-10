#!/bin/bash
# added 2026-08-10 by owen-rote, released under ASL 2.0
#
# intent: an omfile stream must stay usable after a rotation.sizeLimit rotation
# with asyncWriting enabled.
#
# regression guard: doSizeLimitProcessing() reached stopWriter() via
# strmCloseFile(), joining the stream's async writer thread. nothing recreated
# it, so the stream kept queueing buffers with no consumer. the first buffer
# after the rotation was accepted, the next blocked on notFull forever and
# wedged the queue worker driving the action. output stopped after the first
# rotation and shutdown hung.
#
# oracle: seq_check over the concatenated rotated files. every injected message
# must appear exactly once, which requires the stream to keep writing across all
# rotations. the wedge also stalls the queue, so shutdown_when_empty /
# wait_shutdown time out first. no sleeps, no thresholds; the testbench
# timeoutGuard is the hang backstop. mirrors omfile-sizelimitcmd-many.sh, which
# covers the same rotation path with the default synchronous writer.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
echo "mv -f $RSYSLOG_DYNNAME.channel.log.prev.9 $RSYSLOG_DYNNAME.channel.log.prev.10 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.8 $RSYSLOG_DYNNAME.channel.log.prev.9 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.7 $RSYSLOG_DYNNAME.channel.log.prev.8 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.6 $RSYSLOG_DYNNAME.channel.log.prev.7 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.5 $RSYSLOG_DYNNAME.channel.log.prev.6 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.4 $RSYSLOG_DYNNAME.channel.log.prev.5 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.3 $RSYSLOG_DYNNAME.channel.log.prev.4 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.2 $RSYSLOG_DYNNAME.channel.log.prev.3 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev.1 $RSYSLOG_DYNNAME.channel.log.prev.2 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log.prev   $RSYSLOG_DYNNAME.channel.log.prev.1 2>/dev/null
mv -f $RSYSLOG_DYNNAME.channel.log $RSYSLOG_DYNNAME.channel.log.prev
"   > $RSYSLOG_DYNNAME.rotate.sh
chmod +x $RSYSLOG_DYNNAME.rotate.sh
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	action(type="omfile" file="'$RSYSLOG_DYNNAME.channel.log'" template="outfmt"
		asyncWriting="on"
		rotation.sizeLimit="50k"
		rotation.sizeLimitCommand="./'$RSYSLOG_DYNNAME.rotate.sh'")
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
ls -l $RSYSLOG_DYNNAME.channel.*
cat $RSYSLOG_DYNNAME.channel.* > $RSYSLOG_OUT_LOG
seq_check
exit_test
