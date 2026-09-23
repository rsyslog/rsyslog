#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.
# Released under ASL 2.0

## run_fuzz_smoke.sh
## Replay in-tree fuzz seeds against the AFL++ file-mode and libFuzzer targets.
##
## The script assumes the tree is already configured and built. It does not
## start rsyslogd, open network sockets, or write outside the build tree except
## for sanitizer/coverage runtime files produced by the compiler.

set -eu

usage() {
	printf 'usage: %s [file|libfuzzer|all]\n' "$0" >&2
	exit 2
}

mode=${1:-all}
case "$mode" in
	file|libfuzzer|all) ;;
	*) usage ;;
esac

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
seed_dir=${FUZZ_SEED_DIR:-"$repo_dir/fuzz/testcases/test_inputs"}
file_target=${FUZZ_FILE_TARGET:-"$repo_dir/fuzz/fuzz_rsyslog_parsers"}
libfuzzer_target=${FUZZ_LIBFUZZER_TARGET:-"$repo_dir/fuzz/fuzz_rsyslog_parsers_libfuzzer"}
timeout_s=${FUZZ_TIMEOUT:-10}
coverage_dir=${FUZZ_COVERAGE_DIR:-"$repo_dir/fuzz/coverage"}
coverage_object_dir=${FUZZ_COVERAGE_OBJECT_DIR:-}

run_seed_file_mode() {
	seed=$1
	printf 'FUZZ file-mode seed: %s\n' "$seed"
	timeout "$timeout_s" "$file_target" "$seed"
}

run_file_mode_lifecycle_sequences() {
	a="$seed_dir/simple_test.txt"
	b="$seed_dir/cfg_legacy_cmd.txt"
	printf 'FUZZ file-mode lifecycle: A -> A\n'
	timeout "$timeout_s" "$file_target" "$a" "$a"
	printf 'FUZZ file-mode lifecycle: A -> B -> A\n'
	timeout "$timeout_s" "$file_target" "$a" "$b" "$a"
}

run_seed_libfuzzer_mode() {
	seed=$1
	printf 'FUZZ libFuzzer seed: %s\n' "$seed"
	timeout "$timeout_s" "$libfuzzer_target" -runs=1 "$seed"
}

validate_dicts() {
	if [ -f "$repo_dir/fuzz/Makefile" ]; then
		make -C "$repo_dir/fuzz" validate-fuzz-dicts
	else
		"$repo_dir/fuzz/validate_dicts.sh" \
			"$repo_dir/fuzz/rsyslog.dict" \
			"$repo_dir/fuzz/dicts/json.dict" \
			"$repo_dir/fuzz/dicts/parser.dict" \
			"$repo_dir/fuzz/dicts/queue.dict" \
			"$repo_dir/fuzz/dicts/tcp.dict" \
			"$repo_dir/fuzz/dicts/template.dict" \
			"$repo_dir/fuzz/dicts/imfile.dict" \
			"$repo_dir/fuzz/dicts/action.dict"
	fi
}

collect_coverage() {
	[ "${FUZZ_COLLECT_COVERAGE:-0}" = "1" ] || return 0
	if [ -z "$coverage_object_dir" ] || [ ! -d "$coverage_object_dir" ]; then
		printf '%s\n' \
			'FUZZ_COVERAGE_OBJECT_DIR must name the dedicated coverage build tree' >&2
		return 1
	fi
	mkdir -p "$coverage_dir"
	gcov_tool=${GCOV_TOOL:-}
	if [ -z "$gcov_tool" ]; then
		case "${CC:-}" in
			*clang*) gcov_tool="llvm-cov gcov" ;;
			*) gcov_tool=gcov ;;
		esac
	fi
	if command -v lcov >/dev/null 2>&1; then
		case "$gcov_tool" in
			"llvm-cov gcov")
				lcov --capture --directory "$coverage_object_dir" \
					--gcov-tool llvm-cov --gcov-tool gcov \
					--output-file "$coverage_dir/fuzz-coverage.info" \
					--ignore-errors mismatch --ignore-errors negative --filter range
				;;
			*)
				lcov --capture --directory "$coverage_object_dir" \
					--gcov-tool "$gcov_tool" \
					--output-file "$coverage_dir/fuzz-coverage.info" \
					--ignore-errors mismatch --ignore-errors negative --filter range
				;;
		esac
		return 0
	fi

	if [ "$gcov_tool" = "llvm-cov gcov" ] && ! command -v llvm-cov >/dev/null 2>&1; then
		printf 'clang coverage requested but llvm-cov is unavailable; skipping gcov summary\n' \
			> "$coverage_dir/gcov-summary.txt"
		return 0
	fi
	: > "$coverage_dir/gcov-summary.txt"
	find "$coverage_object_dir" -name '*.gcno' -print | sort | while IFS= read -r gcno; do
		gcno_dir=$(dirname -- "$gcno")
		gcno_base=$(basename -- "$gcno")
		(
			cd "$gcno_dir"
			if [ "$gcov_tool" = "llvm-cov gcov" ]; then
				llvm-cov gcov -b -c "$gcno_base"
			else
				"$gcov_tool" -b -c "$gcno_base"
			fi
		) >> "$coverage_dir/gcov-summary.txt" 2>&1
	done
}

clean_coverage_counters() {
	[ "${FUZZ_COLLECT_COVERAGE:-0}" = "1" ] || return 0
	if [ -z "$coverage_object_dir" ] || [ ! -d "$coverage_object_dir" ]; then
		printf '%s\n' \
			'FUZZ_COVERAGE_OBJECT_DIR must name the dedicated coverage build tree' >&2
		return 1
	fi
	find "$coverage_object_dir" -name '*.gcda' -exec rm -f {} +
}

if [ ! -d "$seed_dir" ]; then
	printf 'missing seed directory: %s\n' "$seed_dir" >&2
	exit 1
fi

validate_dicts
clean_coverage_counters

if [ "$mode" = "file" ] || [ "$mode" = "all" ]; then
	if [ ! -x "$file_target" ]; then
		printf 'missing executable file-mode target: %s\n' "$file_target" >&2
		exit 1
	fi
	find "$seed_dir" -type f -print | sort | while IFS= read -r seed; do
		run_seed_file_mode "$seed"
	done
	run_file_mode_lifecycle_sequences
fi

if [ "$mode" = "libfuzzer" ] || [ "$mode" = "all" ]; then
	if [ ! -x "$libfuzzer_target" ]; then
		printf 'missing executable libFuzzer target: %s\n' "$libfuzzer_target" >&2
		exit 1
	fi
	find "$seed_dir" -type f -print | sort | while IFS= read -r seed; do
		run_seed_libfuzzer_mode "$seed"
	done
fi

collect_coverage
