#!/bin/bash
# This test tests impstats omfwd counters in TCP mode
# added 2025-03-17 by Croppi. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export STATSFILE="$RSYSLOG_DYNNAME.stats"
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'"
	interval="1" ruleset="stats" log.syslog="off")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="stats") {
	stop # nothing to do here
}

module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$RSYSLOG_DYNNAME'-testbench_socket" RateLimit.Interval="10" RateLimit.Burst="750" 
)

if $msg contains "msgnum:" then {
	:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
# 1000 messages should be enough
seq 1 1000 | sed 's/^/Test message /' | logger -d -u $RSYSLOG_DYNNAME-testbench_socket

shutdown_when_empty
wait_shutdown

cat -n $STATSFILE

# We submitted 1000 messages within 10 seconds, so we should have 750 messages in the queue and 250 discarded
content_check --regex "imuxsock: origin=imuxsock submitted=750 ratelimit.discarded=250 ratelimit.numratelimiters=1" "$STATSFILE"

exit_test
