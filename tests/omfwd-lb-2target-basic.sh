#!/bin/bash
# added 2024-02-24 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export NUMMESSAGES=1000  # MUST be an EVEN number!

# starting minitcpsrvr receives so that we can obtain their port
# numbers
start_minitcpsrvr $RSYSLOG_OUT_LOG  1
start_minitcpsrvr $RSYSLOG2_OUT_LOG 2

# regular startup
add_conf '
$MainMsgQueueTimeoutShutdown 10000

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then {
	action(type="omfwd" target=["127.0.0.1", "127.0.0.1"]
	                    port=["'$MINITCPSRVR_PORT1'", "'$MINITCPSRVR_PORT2'"]
		protocol="tcp"
		pool.resumeInterval="10"
		action.resumeRetryCount="-1" action.resumeInterval="5")
}
'

# now do the usual run
startup
injectmsg
shutdown_when_empty
wait_shutdown

if [ "$(wc -l < $RSYSLOG_OUT_LOG)" != "$(( NUMMESSAGES / 2 ))" ]; then
	echo "ERROR: RSYSLOG_OUT_LOG has invalid number of messages $(( NUMMESSAGES / 2 ))"
	error_exit 100
fi

if [ "$(wc -l < $RSYSLOG2_OUT_LOG)" != "$(( NUMMESSAGES / 2 ))" ]; then
	echo "ERROR: RSYSLOG2_OUT_LOG has invalid number of messages $(( NUMMESSAGES / 2 ))"
	error_exit 100
fi

# combine both files to check for correct message content
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.log-combined"
cat "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG" > "$SEQ_CHECK_FILE"

seq_check
exit_test
