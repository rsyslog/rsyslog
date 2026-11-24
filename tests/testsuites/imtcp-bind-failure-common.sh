#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Shared process-aware startup and post-readiness survival helpers.
# The caller supplies daemon and modules, plus testbench timeout/tool globals.
# shellcheck disable=SC2154
# Own and reap a bounded child even if it fails its readiness predicate.
stop_bounded_child() {
	local pid=$1 elapsed
	kill "$pid" 2>/dev/null || :
	for ((elapsed = 0; elapsed < 2; elapsed++)); do
		if ! kill -0 "$pid" 2>/dev/null; then
			break
		fi
		sleep 1
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill -KILL "$pid" 2>/dev/null || :
	fi
	wait "$pid" 2>/dev/null || :
}

# Keep the startup bound portable to macOS, where GNU timeout is unavailable.
# Return 124 after the survival deadline, or 125 if requested readiness fails.
# Both paths stop and reap the owned child before returning.
run_bounded() {
	local seconds=$1
	local config=$2
	local log=$3
	local ready_file=${4:-}
	local ready_diagnostic=${5:-}
	local pid elapsed status
	"$daemon" -n -iNONE -f "$config" -M "$modules" > "$log" 2>&1 &
	pid=$!
	if [ -n "$ready_file" ]; then
		# The shared helper counts 100 ms polls. Keep startup separate from
		# the intentional post-readiness survival interval below.
		if ! (error_exit() { exit "$1"; }; wait_file_exists_for_process \
			"$ready_file" "$pid" "$TB_TIMEOUT_STARTSTOP" \
			"imtcp missing-ruleset fallback" "$log"); then
			stop_bounded_child "$pid"
			return 125
		fi
	fi
	if [ -n "$ready_diagnostic" ]; then
		local polls=0
		echo "waiting for startup diagnostic: $ready_diagnostic"
		while ! grep -Fq -- "$ready_diagnostic" "$log"; do
			if ! kill -0 "$pid" 2>/dev/null; then
				echo "child exited before startup diagnostic: $ready_diagnostic"
				stop_bounded_child "$pid"
				return 125
			fi
			if [ "$polls" -ge "$TB_TIMEOUT_STARTSTOP" ]; then
				echo "timeout waiting for startup diagnostic: $ready_diagnostic"
				stop_bounded_child "$pid"
				return 125
			fi
			"$TESTTOOL_DIR/msleep" 100
			((polls++))
		done
	fi
	for ((elapsed = 0; elapsed < seconds; elapsed++)); do
		if ! kill -0 "$pid" 2>/dev/null; then
			wait "$pid"
			return $?
		fi
		sleep 1
	done
	if kill -0 "$pid" 2>/dev/null; then
		stop_bounded_child "$pid"
		return 124
	fi
	wait "$pid"
	status=$?
	return "$status"
}
