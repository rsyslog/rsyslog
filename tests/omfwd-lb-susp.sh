#!/bin/bash
# added 2024-02-19 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export STATSFILE="$RSYSLOG_DYNNAME.stats"
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["xlocalhost", "127.0.0.1"] port="'$TCPFLOOD_PORT'" protocol="tcp")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt"
	       action.ExecOnlyWhenPreviousIsSuspended="on")
}
'
echo Note: intentionally not started any local TCP receiver!

# now do the usual run
startup
injectmsg 0 5000
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

seq_check 0 4999
exit_test
