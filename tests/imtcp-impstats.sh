#!/bin/bash
# added 2025-02-27 by RGerhards, released under ASL 2.0
# This checks primarily that impstats and imtcp work together. There is little
# we can tell about the actual stats.
. ${srcdir:=.}/diag.sh init
export NUM_WORKERS=${NUM_WORKERS:-4}
export NUMMESSAGES=40000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")

input(type="imtcp" name="pstats-test" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	workerthreads="'$NUM_WORKERS'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m $NUMMESSAGES
echo sleeping 2secs to ensure we have at least one stats interval
sleep 2
shutdown_when_empty
wait_shutdown

cat -n $STATSFILE | grep 'w./pstats-test'
NUM_STATS=$(grep 'w./pstats-test' "$STATSFILE" | wc -l)
if [ "$NUM_WORKERS" -gt 1 ]; then
    EXPECTED_COUNT=$NUM_WORKERS
else
    EXPECTED_COUNT=0
fi
# Check if the count matches NUM_WORKERS
if [ "$EXPECTED_COUNT" -gt 0 ]; then
    if [ "$NUM_STATS" -le "$EXPECTED_COUNT" ]; then
        echo "ERROR: Expected at most $EXPECTED_COUNT lines, but found $NUM_STATS in pstats"
        error_exit 1
    fi
else
    if [ "$NUM_STATS" -ne "$EXPECTED_COUNT" ]; then
        echo "ERROR: Expected $EXPECTED_COUNT lines, but found $NUM_STATS in pstats"
        error_exit 1
    fi
fi

seq_check
exit_test
