#!/bin/bash
# added 2024-02-19 by rgerhards. Released under ASL 2.0
# This test is not meant to be executed independetly. It just permits
# to be called by different drivers with different io buffer sizes.
# This in turn is needed to test some edge cases.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test is currently not supported on solaris due to too-different timing"
generate_conf
export NUMMESSAGES=2000

# starting minitcpsrvr receivers so that we can obtain their port
# numbers
export MINITCPSRV_EXTRA_OPTS="-D900 -B2 -a -S5"
start_minitcpsrvr $RSYSLOG_OUT_LOG  1

add_conf '
$MainMsgQueueTimeoutShutdown 10000
$MainMsgQueueDequeueBatchSize 100

global(allMessagesToStderr="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt" iobuffer.maxSize="'$OMFWD_IOBUF_SIZE'")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1"] port="'$MINITCPSRVR_PORT1'" protocol="tcp"
		pool.resumeInterval="2"
		action.reportsuspensioncontinuation="on"
		action.resumeRetryCount="-1" action.resumeInterval="3")
}
'
echo Note: intentionally not started any local TCP receiver!

# now do the usual run
startup

injectmsg
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

export SEQ_CHECK_OPTIONS=-d
#permit 100 messages to be lost in this extreme test (-m 100)
seq_check 0 $((NUMMESSAGES-1)) -m100
exit_test
