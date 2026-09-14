#!/bin/bash
# Shared deterministic helpers for the bounded local-queue scenarios.  All
# waits poll an observable impstats snapshot or a test-owned file, never a
# guessed scheduling delay.

localq_wait_stats() {
	local stats_file="$1"
	local record="$2"
	shift 2
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	local line pattern complete
	while [ "$(date +%s)" -le "$deadline" ]; do
		line=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n 1 || true)
		complete=yes
		for pattern in "$@"; do
			case " $line " in
				*" $pattern "*) ;;
				*) complete=no; break ;;
			esac
		done
		if [ "$complete" = yes ]; then
			printf 'local queue stats reached %s: %s\n' "$record" "$line"
			return
		fi
		$TESTTOOL_DIR/msleep 100
	done
	printf 'FAIL: local queue stats for %s did not contain:' "$record"
	printf ' %s' "$@"
	printf '\nlast record: %s\n' "$line"
	cat "$stats_file" 2>/dev/null || true
	error_exit 1
}

# As above, but use ERE field expressions when a counter is deliberately
# nonzero and its exact scheduling-dependent value is not part of the contract.
localq_wait_stats_regex() {
	local stats_file="$1"
	local record="$2"
	shift 2
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	local line pattern complete
	while [ "$(date +%s)" -le "$deadline" ]; do
		line=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n 1 || true)
		complete=yes
		for pattern in "$@"; do
			if ! printf '%s\n' "$line" | grep -Eq "(^| )${pattern}( |$)"; then
				complete=no
				break
			fi
		done
		if [ "$complete" = yes ]; then
			printf 'local queue stats reached %s: %s\n' "$record" "$line"
			return
		fi
		$TESTTOOL_DIR/msleep 100
	done
	printf 'FAIL: local queue stats for %s did not match:' "$record"
	printf ' %s' "$@"
	printf '\nlast record: %s\n' "$line"
	cat "$stats_file" 2>/dev/null || true
	error_exit 1
}

# The callback opens this FIFO for reading after it has published enter_file.
# Opening the writer is therefore an acknowledgement, not a duration-based
# release.  Call only after wait_file_lines has observed the marker.
localq_release_barrier() {
	printf 'release\n' > "$1" || error_exit $?
}
