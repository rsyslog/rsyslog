#!/bin/sh
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
trap 'rm -f "$tmp_list"' EXIT HUP INT TERM

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
			if ! timeout "$timeout_s" "$target" "$testcase"; then
				failures=$((failures + 1))
			fi
			;;
		libfuzzer)
			if ! timeout "$timeout_s" "$target" -runs=1 "$testcase"; then
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
