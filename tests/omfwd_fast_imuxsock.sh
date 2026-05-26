#!/bin/bash
# This test exercises DiscardMark / DiscardSeverity queue settings with omfwd
# fed through an imuxsock input. Success is that the queue statistics show the
# expected high-volume enqueue/discard behavior while a TCP receiver accepts
# enough forwarded traffic for the action to stay active.
# added 2021-09-02 by alorbach. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "We have no ATOMIC BUILTINS, so OverallQueueSize counting of imdiag is NOT threadsafe and the counting will fail on SunOS"

./syslog_caller -fsyslog_inject-l -m0 > /dev/null 2>&1
no_liblogging_stdlog=$?
if [ $no_liblogging_stdlog -ne 0 ];then
  echo "liblogging-stdlog not available - skipping test"
  exit 77
fi

# export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export NUMMESSAGES=100000

export STATSFILE="$RSYSLOG_DYNNAME.stats"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
generate_conf
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1
PORT_RCVR="$MINITCPSRVR_PORT1"
export PORT_RCVR
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

# IMUX Input socket
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket")

# Note: stats module
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1" ruleset="stats")

main_queue(
    queue.timeoutshutdown="10000"
#    queue.timeoutEnqueue="20"
#    queue.discardSeverity="3"
)

ruleset(name="stats") {
	stop # nothing to do here
}

if $msg contains "test message nbr" then
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
# now do the usual run
startup

# Use syslog_caller 
./syslog_caller -m$NUMMESSAGES -C "uxsock:$RSYSLOG_DYNNAME-testbench_socket"
shutdown_when_empty
wait_shutdown
# note: minitcpsrv shuts down automatically if the connection is closed!

# check output file generation, should contain at least 100 logs
check_file_exists "$RSYSLOG_OUT_LOG"
check_file_exists "$STATSFILE"
content_check --regex --output-results "action-1-builtin:omfwd queue: origin=core.queue size=.* enqueued=100000 full=0 discarded.full=0 discarded.nf=.* maxqsize=100" $STATSFILE

exit_test
