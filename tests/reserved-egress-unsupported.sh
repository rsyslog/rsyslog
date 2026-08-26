#!/bin/bash
# Verify reservedBatch rejects disk-assisted and unbounded async targets plus
# a Direct source queue. Each case requires a nonzero -N1 status and its
# specific startup diagnostic; no runtime timing or external service is used.
. ${srcdir:=.}/diag.sh init

validate_rejected() {
  label=$1
  expected=$2
  validation_log="$RSYSLOG_DYNNAME.$label.validation.log"
  if ../tools/rsyslogd -N1 -M../runtime/.libs -f"${TESTCONF_NM}.conf" >"$validation_log" 2>&1; then
    error_exit 1 "reservedBatch accepted unsupported $label queue"
  fi
  content_check "$expected" "$validation_log"
}

generate_conf
add_conf '
global(executionEngine="reservedBatch")
ruleset(name="unsupported" queue.type="LinkedList" queue.filename="reserved") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
validate_rejected disk-assisted \
  "supports only non-disk-assisted bounded LinkedList and FixedArray queued rulesets"

generate_conf
add_conf '
global(executionEngine="reservedBatch")
ruleset(name="unsupported" queue.type="LinkedList" queue.size="0") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
validate_rejected unbounded \
  "supports only non-disk-assisted bounded LinkedList and FixedArray queued rulesets"

generate_conf
add_conf '
global(executionEngine="reservedBatch")
main_queue(queue.type="Direct")
'
validate_rejected direct-main "requires a non-Direct main queue"
exit_test
