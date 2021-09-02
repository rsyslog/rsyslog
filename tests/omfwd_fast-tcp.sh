#!/bin/bash
# This test tries tests DiscardMark / DiscardSeverity queue settings with omfwd with TCP Input
# added 2021-09-02 by alorbach. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export NUMMESSAGES=1000000

export TCPFLOOD_PORT="$(get_free_port)"
export PORT_RCVR="$(get_free_port)"
export STATSFILE="$RSYSLOG_DYNNAME.stats"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
generate_conf
add_conf '
global(	debug.whitelist="on"
	debug.files=["imdiag.c", "queue.c"]
)

template(name="testformat" type="list") {
    constant(value="{ ")
    property(name="timereported" dateFormat="unixtimestamp" format="jsonf")
    constant(value=", ")
    property(name="syslogseverity-text" format="jsonf")
    constant(value=", ")
    property(name="programname" format="jsonf")
    constant(value=", ")
    property(name="msg" format="jsonf")
    constant(value=" }\n")
}
template(name="outfmt" 
	type="string" 
	string="%msg:F,58:2%\n")


# Note: stats module
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1" ruleset="stats")

# Note: listener for tcpflood!
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

main_queue(
    queue.timeoutshutdown="10000"
#    queue.timeoutEnqueue="20"
#    queue.discardSeverity="3"
)

ruleset(name="stats") {
	stop # nothing to do here
}

if $msg contains "msgnum:" then
	action(
	    type="omfwd"
	    Target="127.0.0.1"
	    Port="'$PORT_RCVR'"
	    Protocol="tcp"
	    TCP_Framing="octet-counted"
	    ResendLastMSGOnReconnect="on"

	    Template="testformat"

	    queue.discardMark="100"
	    queue.discardSeverity="3"

#	    queue.timeoutEnqueue="5"
	    queue.type="linkedlist"
	)
'
./minitcpsrv -t127.0.0.1 -p$PORT_RCVR -f $RSYSLOG_OUT_LOG &
BGPROCESS=$!
echo background tcp dummy receiver process id is $BGPROCESS

# now do the usual run
startup
# Use TCPFLOOD
tcpflood -c10 -m$NUMMESSAGES -i1
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

# check output file generation, should contain at least 100 logs
check_file_exists "$RSYSLOG_OUT_LOG"
check_file_exists "$STATSFILE"
content_check --regex --output-results "action-1-builtin:omfwd queue: origin=core.queue size=100 enqueued=1000000 full=0 discarded.full=0 discarded.nf=.* maxqsize=100" $STATSFILE
exit_test
