#!/bin/bash
# Prove the no-pressure path retains target batching. A guaranteed 32-message
# source dequeue batch executes three queued calls per source message to one
# roomy target. Exact delivery proves all 96 branches published; impstats must
# report one publication batch and one publication advice for those 96
# messages. The initial 1.5-second wait lets startup messages pass the main
# queue's one-second minimum-batch timeout before the 32-message stimulus.
. ${srcdir:=.}/diag.sh init
export STATSFILE="$RSYSLOG_DYNNAME.stats"
generate_conf
add_conf '
global(executionEngine="reservedBatch")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off"
       resetCounters="off" log.file="'$STATSFILE'")
main_queue(queue.type="LinkedList" queue.dequeueBatchSize="32"
           queue.minDequeueBatchSize="32" queue.minDequeueBatchSize.timeout="1000")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="128") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then {
  call target
  call target
  set $.target_name = "target";
  call_indirect $.target_name;
}
'
startup
"$TESTTOOL_DIR/msleep" 1500
injectmsg 0 32
wait_file_lines "$RSYSLOG_OUT_LOG" 96

i=0
while ! grep -E 'target: origin=core.queue .*egress\.published\.messages=96 ' \
    "$STATSFILE" >/dev/null 2>&1; do
  i=$((i + 1))
  if [ "$i" -ge 50 ]; then
    echo "expected reserved-egress publication counters for 96 normal-path branches"
    test -f "$STATSFILE" && tail -20 "$STATSFILE"
    error_exit 1
  fi
  "$TESTTOOL_DIR/msleep" 100
done

stats_line=$(grep -E 'target: origin=core.queue .*egress\.published\.messages=96 ' "$STATSFILE" | tail -1)
published_batches=$(printf '%s\n' "$stats_line" | sed -n 's/.*egress\.published\.batches=\([0-9][0-9]*\).*/\1/p')
publication_advice=$(printf '%s\n' "$stats_line" | sed -n 's/.*egress\.publication\.advice=\([0-9][0-9]*\).*/\1/p')
if [ "$published_batches" != 1 ] || [ "$publication_advice" != 1 ]; then
  echo "expected one publication batch/advice for 96 normal-path branches: $stats_line"
  error_exit 1
fi

shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 96 "$RSYSLOG_OUT_LOG"
exit_test
