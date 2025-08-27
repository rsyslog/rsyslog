#!/bin/bash
# add 2018-08-02 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000
echo "ls -l $RSYSLOG_DYNNAME*
mv -f $RSYSLOG_DYNNAME.channel.log $RSYSLOG_DYNNAME.channel.log.prev
"   > $RSYSLOG_DYNNAME.rotate.sh
chmod +x $RSYSLOG_DYNNAME.rotate.sh
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfile" template="outfmt")
$outchannel log_rotation,'$RSYSLOG_DYNNAME.channel.log', 50000,./'$RSYSLOG_DYNNAME.rotate.sh'
:msg, contains, "msgnum:" :omfile:$log_rotation
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
cat $RSYSLOG_DYNNAME.channel.* > $RSYSLOG_OUT_LOG
ls -l $RSYSLOG_DYNNAME*
seq_check
exit_test
