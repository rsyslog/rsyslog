#!/bin/bash
# add 2018-08-02 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=500000
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
main_queue(
	queue.workerthreads="4"
	queue.timeoutWorkerthreadShutdown="-1"
	queue.workerThreadMinimumMessages="10"
)
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="tcp")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfile" template="outfmt")
$outchannel log_rotation,'$RSYSLOG_DYNNAME.channel.log', $NUMMESSAGES,./'$RSYSLOG_DYNNAME.rotate.sh'

if $msg contains "msgnum:" then {
#	if $/num % 2 == 0 then
		:omfile:$log_rotation
#	else
#		:omfile:$log_rotation2
#	set $/num = $/num + 1;
}

ruleset(name="tcp") {
	:omfile:$log_rotation2
}
'
startup
./tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES & # TCPFlood needs to run async!
#./msleep 2500
injectmsg
sleep 1
shutdown_when_empty
wait_shutdown
ls -l $RSYSLOG_DYNNAME.channel.*
cat $RSYSLOG_DYNNAME.channel.* > $RSYSLOG_OUT_LOG
seq_check
exit_test
