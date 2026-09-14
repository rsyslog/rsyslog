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

# Local queues qualify every action, including diag.sh's own startup marker.
# The checker requires that omfile destination to be absolute. Rewrite only
# this generated per-test marker after generate_conf, before add_conf appends
# the local-queue graph.
localq_make_startup_marker_absolute() {
	local config="${TESTCONF_NM}.conf"
	local relative="./${RSYSLOG_DYNNAME}.started"
	local absolute="$PWD/${RSYSLOG_DYNNAME}.started"
	# Define the ordinary template before the generated action. The local graph
	# deliberately rejects the default generated template because it is a
	# generated template rather than the explicit string template contract.
	sed -i '/# Capture rsyslogd own messages/i template(name="localdiag" type="string" string="%msg%\\n")' "$config" || error_exit $?
	sed -i "s|file=\"${relative}\"|file=\"${absolute}\" template=\"localdiag\"|" "$config" || error_exit $?
}
