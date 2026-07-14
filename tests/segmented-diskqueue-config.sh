#!/bin/bash
# Verify that segmentedDisk rejects missing storage identity, unsupported
# corruption/encryption modes, and legacy action syntax. Each case must fail
# -N1 with its specific diagnostic rather than silently changing queue type.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

check_invalid_config() {
	local suffix="$1"
	local expected="$2"
	local log="${RSYSLOG_DYNNAME}.${suffix}.log"
	local conf="${TESTCONF_NM}${suffix}.conf"

	if ../tools/rsyslogd -N1 -f"$conf" -M../runtime/.libs:../.libs >"$log" 2>&1; then
		echo "FAIL: expected segmentedDisk configuration '$suffix' to be rejected"
		cat "$log"
		error_exit 1
	fi
	grep -F "$expected" "$log" >/dev/null || {
		echo "FAIL: expected segmentedDisk configuration diagnostic: $expected"
		cat "$log"
		error_exit 1
	}
}

NO_FILENAME_CONF="${RSYSLOG_DYNNAME}_nofilename"
generate_conf "$NO_FILENAME_CONF"
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" queue.type="segmentedDisk")
' "$NO_FILENAME_CONF"
check_invalid_config "$NO_FILENAME_CONF" "segmentedDisk mode selected, but no queue file name given"

LEGACY_CONF="${RSYSLOG_DYNNAME}_legacy"
generate_conf "$LEGACY_CONF"
add_conf '
$WorkDirectory '${RSYSLOG_DYNNAME}'.spool
$ActionQueueFileName legacyq
$ActionQueueType segmentedDisk
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
' "$LEGACY_CONF"
check_invalid_config "$LEGACY_CONF" '$ActionQueueType does not support the experimental segmentedDisk backend'

EMPTY_SPOOL_CONF="${RSYSLOG_DYNNAME}_empty_spool"
generate_conf "$EMPTY_SPOOL_CONF"
add_conf '
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" queue.type="segmentedDisk"
       queue.filename="emptyq" queue.spoolDirectory="")
' "$EMPTY_SPOOL_CONF"
check_invalid_config "$EMPTY_SPOOL_CONF" "queue.spooldirectory must not be empty"

CORRUPTION_CONF="${RSYSLOG_DYNNAME}_corruption_mode"
generate_conf "$CORRUPTION_CONF"
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" queue.type="segmentedDisk"
       queue.filename="corruptq" queue.onCorruption="ignore")
' "$CORRUPTION_CONF"
check_invalid_config "$CORRUPTION_CONF" 'segmentedDisk currently supports only queue.onCorruption="safe"'

ENCRYPTION_CONF="${RSYSLOG_DYNNAME}_encryption"
generate_conf "$ENCRYPTION_CONF"
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" queue.type="segmentedDisk"
       queue.filename="cryptq" queue.cry.provider="gcry")
' "$ENCRYPTION_CONF"
check_invalid_config "$ENCRYPTION_CONF" "queue.cry.provider is not yet supported by segmentedDisk"

rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
