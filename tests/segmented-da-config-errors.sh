#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Queue DA engine parameters are intentionally limited to FixedArray and
# LinkedList queues with a filename. The permissive oracle is a successful -N1
# validation plus the dirty-configuration diagnostic; the strict oracle is a
# failed validation when abortOnUncleanConfig is enabled.
. ${srcdir:=.}/diag.sh init

assert_contains() {
	grep -Fq "$2" "$1" || {
		cat "$1"
		error_exit 1 "missing expected diagnostic: $2"
	}
}

check_permissive_invalid_context() {
	generate_conf
	add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.diskQueueType="disk" queue.diskQueueAutoUpgrade="on"
	queue.diskQueueIdleTimeout="25")
'
	local out="${RSYSLOG_DYNNAME}.permissive.log"
	export RS_REDIR=">\"$out\" 2>&1"
	startup
	shutdown_when_empty
	wait_shutdown
	unset RS_REDIR
	assert_contains "$out" 'apply only to FixedArray or LinkedList disk-assisted queues'
}

check_strict_invalid_context() {
	generate_conf
	add_conf '
global(abortOnUncleanConfig="on" workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="segmentedDisk" queue.filename="mainq"
	queue.diskQueueType="disk")
'
	local out="${RSYSLOG_DYNNAME}.strict.log"
	if ../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"$out" 2>&1; then
		error_exit 1 "strict invalid-context configuration unexpectedly passed"
	fi
	assert_contains "$out" 'apply only to FixedArray or LinkedList disk-assisted queues'
}

check_strict_meaningless_options() {
	generate_conf
	add_conf '
global(abortOnUncleanConfig="on" workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.diskQueueType="disk" queue.diskQueueAutoUpgrade="on"
	queue.diskQueueIdleTimeout="25")
'
	local out="${RSYSLOG_DYNNAME}.meaningless.log"
	if ../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"$out" 2>&1; then
		error_exit 1 "strict meaningless DA options unexpectedly passed"
	fi
	assert_contains "$out" 'queue.diskQueueAutoUpgrade applies only'
	assert_contains "$out" 'queue.diskQueueIdleTimeout does not apply'
}

check_strict_invalid_values() {
	generate_conf
	add_conf '
global(abortOnUncleanConfig="on" workDirectory="'${RSYSLOG_DYNNAME}'.spool")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.diskQueueType="unknown" queue.diskQueueIdleTimeout="-2")
'
	local out="${RSYSLOG_DYNNAME}.invalid-values.log"
	if ../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"$out" 2>&1; then
		error_exit 1 "strict invalid DA values unexpectedly passed"
	fi
	assert_contains "$out" "queue.diskQueueType: invalid value 'unknown'"
	assert_contains "$out" 'queue.diskQueueIdleTimeout must be -1 or greater'
}

check_permissive_invalid_context
check_strict_invalid_context
check_strict_meaningless_options
check_strict_invalid_values
exit_test
