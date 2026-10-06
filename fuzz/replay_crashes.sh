#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.
# Released under ASL 2.0

## replay_crashes.sh
## Replay crash files against the rsyslog fuzz targets under the current build.
##
## The script accepts either individual files or directories. Directory inputs
## are traversed recursively. A non-zero exit means at least one replay failed,
## timed out, or reproduced a sanitizer/crash exit status.

set -eu

usage() {
	cat >&2 <<'USAGE'
usage: fuzz/replay_crashes.sh --engine file|libfuzzer --target PATH [--timeout SEC] PATH...
USAGE
	exit 2
}

engine=
target=
timeout_s=10

while [ "$#" -gt 0 ]; do
	case "$1" in
		--engine) engine=${2:-}; shift 2 ;;
		--target) target=${2:-}; shift 2 ;;
		--timeout) timeout_s=${2:-}; shift 2 ;;
		-h|--help) usage ;;
		*) break ;;
	esac
done

[ -n "$engine" ] || usage
[ -n "$target" ] || usage
[ "$#" -gt 0 ] || usage
[ -x "$target" ] || {
	printf 'target is not executable: %s\n' "$target" >&2
	exit 1
}

export ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}
export MSAN_OPTIONS=${MSAN_OPTIONS:-halt_on_error=1}

tmp_list=$(mktemp "${TMPDIR:-/tmp}/rsyslog-fuzz-replay.XXXXXX")
active_pid=
cleanup() {
	rm -f "$tmp_list"
}
terminate() {
	signal=$1
	exit_code=$2
	if [ -n "$active_pid" ]; then
		kill -"$signal" "$active_pid" 2>/dev/null || :
		wait "$active_pid" 2>/dev/null || :
	fi
	exit "$exit_code"
}
run_replay() {
	timeout "$timeout_s" "$@" &
	active_pid=$!
	if wait "$active_pid"; then
		result=0
	else
		result=$?
	fi
	active_pid=
	return "$result"
}
trap cleanup EXIT
trap 'terminate HUP 129' HUP
trap 'terminate INT 130' INT
trap 'terminate TERM 143' TERM

for path in "$@"; do
	if [ -d "$path" ]; then
		find "$path" -type f -print >> "$tmp_list"
	elif [ -f "$path" ]; then
		printf '%s\n' "$path" >> "$tmp_list"
	else
		printf 'missing replay input: %s\n' "$path" >&2
		exit 1
	fi
done

failures=0
while IFS= read -r testcase; do
	[ -n "$testcase" ] || continue
	printf 'REPLAY %s\n' "$testcase"
	case "$engine" in
		file)
			if ! run_replay "$target" "$testcase"; then
				failures=$((failures + 1))
			fi
			;;
		libfuzzer)
			if ! run_replay "$target" -runs=1 "$testcase"; then
				failures=$((failures + 1))
			fi
			;;
		*) usage ;;
	esac
done < "$tmp_list"

if [ "$failures" -ne 0 ]; then
	printf 'replay failures: %d\n' "$failures" >&2
	exit 1
fi
