#!/bin/bash
# added 2026-08-10 by Owen Rotenberg, released under ASL 2.0
#
# Intent: guard the invariant that an omfile stream stays usable after a
# rotation.sizeLimit rotation when asyncWriting is enabled.
#
# Regression guard: doSizeLimitProcessing() used to reach stopWriter() via
# strmCloseFile(), which joined the stream's async writer thread. Nothing ever
# recreated it, so the stream kept accepting buffers into a queue with no
# consumer. The first buffer handed over after the rotation was accepted and the
# next one blocked forever on the notFull condition, wedging the queue worker
# that drives the action. Output stopped after the first rotation and shutdown
# hung.
#
# Oracle: seq_check over the concatenated rotated files. Every injected message
# must be present exactly once, which can only happen if the stream keeps
# writing across all rotations. The wedge fails this deterministically: it also
# stalls the queue, so shutdown_when_empty/wait_shutdown time out first. No
# sleeps or thresholds are involved; the testbench timeoutGuard remains the hang
# backstop. This mirrors omfile-sizelimitcmd-many.sh, which covers the same
# rotation path with the default synchronous writer.
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
