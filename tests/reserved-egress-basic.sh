#!/bin/bash
# Verify the opt-in reserved-batch engine publishes snapshots from conditional
# call and foreach/call_indirect statements to LinkedList and FixedArray targets.
# An explicitly configured Direct ruleset must execute synchronously and mutate
# the current message before the queued snapshots are taken, while the source
# direct action retains its normal result. Exact output is the oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
template(name="snap" type="string" string="%msg%|%$!snap%|%$!direct_ruleset%\n")

ruleset(name="explicit_direct" queue.type="Direct") {
  set $!direct_ruleset = "synchronous";
}

ruleset(name="linked" queue.type="LinkedList" queue.size="1" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.linked" template="snap")
}
ruleset(name="fixed" queue.type="FixedArray" queue.size="1" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.fixed" template="snap")
}

if $msg contains "msgnum" then {
  set $!snap = "encounter";
  call explicit_direct
  call linked
  set $.parse_result = parse_json('\''["fixed"]'\'', "\$!routes");
  foreach ($.route in $!routes) do {
    call_indirect $.route;
  }
  set $!snap = "mutated";
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.direct" template="snap")
}
'
startup
injectmsg 0 5
issue_HUP
injectmsg 5 5
shutdown_when_empty
wait_shutdown
content_count_check "encounter" 10 "$RSYSLOG_OUT_LOG.linked"
content_count_check "encounter" 10 "$RSYSLOG_OUT_LOG.fixed"
content_count_check "mutated" 10 "$RSYSLOG_OUT_LOG.direct"
content_count_check "synchronous" 10 "$RSYSLOG_OUT_LOG.linked"
content_count_check "synchronous" 10 "$RSYSLOG_OUT_LOG.fixed"
content_count_check "synchronous" 10 "$RSYSLOG_OUT_LOG.direct"
exit_test
