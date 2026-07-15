#!/bin/bash
# Verify YAML uses the same DA parameter validation as RainerScript. A
# segmented-only queue is an invalid context: permissive validation must retain
# startup compatibility while reporting the dirty configuration, and strict
# validation must reject it. Explicit classic options that have no effect are
# likewise rejected in strict mode.
. ${srcdir:=.}/diag.sh init

assert_contains() {
	grep -Fq "$2" "$1" || {
		cat "$1"
		error_exit 1 "missing expected YAML diagnostic: $2"
	}
}

write_loader() {
	generate_conf
	add_conf 'include(file="'${TESTCONF_NM}'.yaml")'
	rm -f "${TESTCONF_NM}.yaml"
}

validate_permissive_context() {
	write_loader
	add_yaml_conf 'global:'
	add_yaml_conf '  workDirectory: "'${RSYSLOG_DYNNAME}'.spool"'
	add_yaml_conf 'mainqueue:'
	add_yaml_conf '  queue.type: segmentedDisk'
	add_yaml_conf '  queue.filename: mainq'
	add_yaml_conf '  queue.diskQueueType: disk'
	add_yaml_conf '  queue.diskQueueAutoUpgrade: on'
	add_yaml_conf '  queue.diskQueueIdleTimeout: 25'
	local out="${RSYSLOG_DYNNAME}.yaml-permissive.log"
	export RS_REDIR=">\"$out\" 2>&1"
	startup
	shutdown_when_empty
	wait_shutdown
	unset RS_REDIR
	assert_contains "$out" 'apply only to FixedArray or LinkedList disk-assisted queues'
}

validate_strict_options() {
	write_loader
	add_yaml_conf 'global:'
	add_yaml_conf '  abortOnUncleanConfig: on'
	add_yaml_conf '  workDirectory: "'${RSYSLOG_DYNNAME}'.spool"'
	add_yaml_conf 'mainqueue:'
	add_yaml_conf '  queue.type: FixedArray'
	add_yaml_conf '  queue.filename: mainq'
	add_yaml_conf '  queue.diskQueueType: disk'
	add_yaml_conf '  queue.diskQueueAutoUpgrade: on'
	add_yaml_conf '  queue.diskQueueIdleTimeout: 25'
	local out="${RSYSLOG_DYNNAME}.yaml-strict.log"
	if ../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"$out" 2>&1; then
		error_exit 1 "strict meaningless YAML DA options unexpectedly passed"
	fi
	assert_contains "$out" 'queue.diskQueueAutoUpgrade applies only'
	assert_contains "$out" 'queue.diskQueueIdleTimeout does not apply'
}

validate_permissive_context
validate_strict_options
exit_test
