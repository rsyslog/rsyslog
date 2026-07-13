#!/bin/bash
# Fill a bounded segmentedDisk queue while the consumer is slowed, then drain
# and enqueue a second wave. Complete delivery proves producer backpressure,
# worker wakeup when a multi-submit blocks at the disk limit, and post-delete
# reuse. Like the classic disk queue, the current record may cross the byte
# limit; the oracle permits one bounded record/topology overshoot.
. ${srcdir:=.}/diag.sh init
require_plugin impstats
export NUMMESSAGES=2000
STATS_FILE="$PWD/${RSYSLOG_DYNNAME}.stats.log"
generate_conf
add_conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATS_FILE'" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.maxDiskSpace="64k" queue.maxFileSize="8k"
	queue.timeoutEnqueue="300000" queue.dequeueBatchSize="16")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" :omtesting:sleep 0 1000
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -p"$TCPFLOOD_PORT" -m1000 -i0
wait_file_lines "$RSYSLOG_OUT_LOG" 1000
tcpflood -p"$TCPFLOOD_PORT" -m1000 -i1000
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
max_usage=$(grep 'disk.usage=' "$STATS_FILE" | sed -E 's/.*disk\.usage=([0-9]+).*/\1/' |
	sort -n | tail -n 1)
if [ "${max_usage:-0}" -gt 66560 ]; then
	printf 'FAIL: segmentedDisk sampled usage %s exceeds the 64 KiB limit plus one record\n' "$max_usage"
	cat "$STATS_FILE"
	error_exit 1
fi
rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
