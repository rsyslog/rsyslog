#!/bin/bash
# add 2023-01-11 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
echo "ls -l $RSYSLOG_DYNNAME.channel.*
mv -f $RSYSLOG_DYNNAME.channel.log.prev.9 $RSYSLOG_DYNNAME.channel.log.prev.10 2>/dev/null
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
