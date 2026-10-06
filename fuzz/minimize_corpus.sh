#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.
# Released under ASL 2.0

## minimize_corpus.sh
## Minimize a fuzz corpus with AFL++ afl-cmin or libFuzzer merge mode.
##
## Examples:
##   fuzz/minimize_corpus.sh --engine afl --target ./fuzz/fuzz_rsyslog_parsers \
##       --input fuzz/testcases/test_inputs --output /tmp/rsyslog-cmin \
##       --dict fuzz/dicts/parser.dict
##   fuzz/minimize_corpus.sh --engine libfuzzer --target ./fuzz/fuzz_rsyslog_parsers_libfuzzer \
##       --input corpus/full --output corpus/min

set -eu

usage() {
	cat >&2 <<'USAGE'
usage: fuzz/minimize_corpus.sh --engine afl|libfuzzer --target PATH --input DIR --output DIR [--dict PATH] [--timeout SEC]
USAGE
	exit 2
}

engine=
target=
input=
output=
dict=
timeout_s=120

while [ "$#" -gt 0 ]; do
	case "$1" in
		--engine) engine=${2:-}; shift 2 ;;
		--target) target=${2:-}; shift 2 ;;
		--input) input=${2:-}; shift 2 ;;
		--output) output=${2:-}; shift 2 ;;
		--dict) dict=${2:-}; shift 2 ;;
		--timeout) timeout_s=${2:-}; shift 2 ;;
		-h|--help) usage ;;
		*) usage ;;
	esac
done

[ -n "$engine" ] || usage
[ -n "$target" ] || usage
[ -n "$input" ] || usage
[ -n "$output" ] || usage
[ -x "$target" ] || {
	printf 'target is not executable: %s\n' "$target" >&2
	exit 1
}
[ -d "$input" ] || {
	printf 'input corpus is not a directory: %s\n' "$input" >&2
	exit 1
}
if [ -n "$dict" ] && [ ! -f "$dict" ]; then
	printf 'dictionary does not exist: %s\n' "$dict" >&2
	exit 1
fi
case "$timeout_s" in
	''|*[!0-9]*|0)
		printf 'timeout must be a positive number of seconds: %s\n' "$timeout_s" >&2
		exit 2
		;;
esac

mkdir -p "$output"

case "$engine" in
	afl)
		afl_cmin=${AFL_CMIN:-afl-cmin}
		command -v "$afl_cmin" >/dev/null 2>&1 || {
			printf 'missing afl-cmin; install AFL++ or set AFL_CMIN\n' >&2
			exit 1
		}
		timeout_ms=$((timeout_s * 1000))
		if [ -n "$dict" ]; then
			"$afl_cmin" -i "$input" -o "$output" -m none -t "$timeout_ms" \
				-x "$dict" -- "$target" @@
		else
			"$afl_cmin" -i "$input" -o "$output" -m none -t "$timeout_ms" \
				-- "$target" @@
		fi
		;;
	libfuzzer)
		if [ -n "$dict" ]; then
			"$target" -merge=1 "$output" "$input" -max_total_time="$timeout_s" \
				"-dict=$dict"
		else
			"$target" -merge=1 "$output" "$input" -max_total_time="$timeout_s"
		fi
		;;
	*) usage ;;
esac
