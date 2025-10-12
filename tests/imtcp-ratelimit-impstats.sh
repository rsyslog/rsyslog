#!/bin/bash
# Validate impstats reporting for imtcp with shared ratelimit configuration.
# Added 2025-XX-XX, released under ASL 2.0.

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=200
export BURST=5
export STATSFILE="$RSYSLOG_DYNNAME.stats"
export RATELIMIT_LOG="$RSYSLOG_DYNNAME.ratelimit.log"
export RATELIMIT_EXPECTED_LINES=$BURST
export RATELIMIT_OUTPUT_FILE="$RSYSLOG_OUT_LOG"
export RATELIMIT_WAIT_GRACE_SECS=5
export RATELIMIT_SETTLE_SECS=1
export QUEUE_EMPTY_CHECK_FUNC=wait_ratelimit_lines
export RATELIMIT_BURST_LIMIT=$BURST

generate_conf
add_conf '
ratelimit(name="tcp_stats" interval="1" burst="'${BURST}'")

module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'${STATSFILE}'" interval="1"
	ruleset="stats" log.syslog="off")

ruleset(name="stats") {
	stop
}

input(type="imtcp" name="tcp-stats" port="0"
	listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
	ratelimit.name="tcp_stats")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'${RSYSLOG_OUT_LOG}'")

:msg, contains, "begin to drop messages due to rate-limiting" action(type="omfile"
			         file="'${RATELIMIT_LOG}'")
'

startup
tcpflood -m $NUMMESSAGES
sleep 2
shutdown_when_empty
wait_shutdown

assert_ratelimit_delivery

content_check --regex " origin=imtcp.* submitted=$NUMMESSAGES" "$STATSFILE"
content_check --regex "tcp-stats from <.*>: begin to drop messages due to rate-limiting" "$RATELIMIT_LOG"

exit_test
