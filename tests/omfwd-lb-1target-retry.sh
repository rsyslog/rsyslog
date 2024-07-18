#!/bin/bash
# added 2024-02-19 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=1000

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
export MINITCPSRV_EXTRA_OPTS="-D900"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1

add_conf '
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueDequeueBatchSize 100

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

./all.tmp
if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1"] port="'$MINITCPSRVR_PORT1'" protocol="tcp"
		pool.resumeInterval="1"
		action.resumeRetryCount="-1" action.resumeInterval="1")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt"
	       action.ExecOnlyWhenPreviousIsSuspended="on")
}
'
echo Note: intentionally not started any local TCP receiver!

# now do the usual run
startup

injectmsg
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

cp $RSYSLOG_OUT_LOG  tmp
seq_check 0 $((NUMMESSAGES-1)) -m20 #permit 10 messages to be lost
exit_test
