#!/bin/bash
# Verify that each core queue reports mutex contention and accumulated wait
# time through impstats while a multi-connection imtcp workload is fully
# delivered.  The oracle requires the metric keys and all messages; contention
# itself may be zero on a constrained or lightly scheduled runner. The metric
# keys and complete delivery prove that the per-queue diagnostic option is
# active without making scheduling-dependent contention an oracle.
. ${srcdir:=.}/diag.sh init
require_plugin impstats
export NUMMESSAGES=20000
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerthreads="4")
main_queue(queue.workerThreads="4" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.mutexContentionStats="on")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -c16 -m"$NUMMESSAGES"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
wait_file_lines "$STATSFILE" 1
shutdown_when_empty
wait_shutdown
content_check --regex --output-results \
	'main Q: origin=core.queue .*mutex\.contention=[0-9][0-9]* .*mutex\.wait_ns=[0-9][0-9]*' "$STATSFILE"
seq_check
exit_test
