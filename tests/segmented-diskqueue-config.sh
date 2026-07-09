#!/bin/bash
# Verify that the experimental segmentedDisk backend rejects configurations
# that omit its filename or use the legacy action-queue directive. The oracle
# is a failed -N1 check with the specific configuration diagnostic in each case.
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

rm -rf "${RSYSLOG_DYNNAME}.spool"
exit_test
