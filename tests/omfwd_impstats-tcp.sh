#!/bin/bash
# This test tests impstats omfwd counters in TCP mode
# added 2021-02-11 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export STATSFILE="$RSYSLOG_DYNNAME.stats"
add_conf '
$MainMsgQueueTimeoutShutdown 10000


module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'"
	interval="1" ruleset="stats")

ruleset(name="stats") {
	stop # nothing to do here
}

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then
	action(type="omfwd" target="127.0.0.1" port="'$TCPFLOOD_PORT'" protocol="tcp")
'
./minitcpsrv -t127.0.0.1 -p$TCPFLOOD_PORT -f $RSYSLOG_OUT_LOG &
BGPROCESS=$!
echo background minitcpsrv process id is $BGPROCESS

# now do the usual run
startup
# 10000 messages should be enough
injectmsg 0 10000
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

cat -n $STATSFILE
# check pstats - that's our prime test target
content_check --regex "TCP-.*origin=omfwd .*bytes.sent=[1-9][0-9][0-9]" "$STATSFILE"

# do a bonus test while we are at it (just make sure we actually sent data)
seq_check 0 9999
exit_test
