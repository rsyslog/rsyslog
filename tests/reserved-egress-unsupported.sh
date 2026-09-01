#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify the conf-mode RSTB_EXECUTION_ENGINE preamble selects reservedBatch and
# rejects disk-assisted and unbounded async targets plus a Direct source queue.
# It also verifies an invalid selector returns a configuration error through the
# normal LogError path. Each case requires a nonzero -N1 status and its specific
# startup diagnostic; no runtime timing or external service is used.
. ${srcdir:=.}/diag.sh init
export RSTB_EXECUTION_ENGINE=reservedBatch

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
ruleset(name="unsupported" queue.type="LinkedList" queue.filename="reserved") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
validate_rejected disk-assisted \
  "supports only non-disk-assisted bounded LinkedList and FixedArray queued rulesets"

generate_conf
add_conf '
ruleset(name="unsupported" queue.type="LinkedList" queue.size="0") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
'
validate_rejected unbounded \
  "supports only non-disk-assisted bounded LinkedList and FixedArray queued rulesets"

generate_conf
add_conf '
main_queue(queue.type="Direct")
'
validate_rejected direct-main "requires a non-Direct main queue"

RSTB_EXECUTION_ENGINE=notAnEngine
generate_conf
validate_rejected invalid-selector \
  "invalid global executionEngine 'notAnEngine'; expected 'legacy' or 'reservedBatch'"
exit_test
