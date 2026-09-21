#!/bin/sh
## ci_casr_triage.sh
## Rebuild the AFL++ target and triage downloaded AFL++ artifacts with CASR.

set -eu

repo_dir=${CI_PROJECT_DIR:-/src}
cd "$repo_dir"

. "$repo_dir/fuzz/ci_fuzz_common.sh"

afl_root=${FUZZ_OUT_ROOT:-artifacts/out}
casr_root=${CASR_OUT_ROOT:-artifacts/casr}
casr_timeout=${CASR_TIMEOUT:-10}

mkdir -p "$casr_root"

if [ ! -d "$afl_root" ]; then
	printf 'missing AFL++ artifact directory: %s\n' "$afl_root" >&2
	printf 'missing AFL++ artifact directory: %s\n' "$afl_root" > "$casr_root/NO_AFL_OUTPUT.txt"
	exit 1
fi

found_target=0
for afl_dir in "$afl_root"/*; do
	[ -d "$afl_dir" ] || continue
	found_target=1
done

if [ "$found_target" -eq 0 ]; then
	printf 'no AFL++ target output directories found under %s\n' "$afl_root" >&2
	printf 'no AFL++ target output directories found under %s\n' "$afl_root" > "$casr_root/NO_AFL_OUTPUT.txt"
	exit 1
fi

export AFL_IGNORE_PROBLEMS=${AFL_IGNORE_PROBLEMS:-1}
export AFL_IGNORE_PROBLEMS_COVERAGE=${AFL_IGNORE_PROBLEMS_COVERAGE:-1}
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1:symbolize=1}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}

if [ ! -x ./fuzz/fuzz_rsyslog_parsers ]; then
	printf 'missing build-stage fuzz binary: %s\n' ./fuzz/fuzz_rsyslog_parsers >&2
	exit 1
fi
ci_set_afl_preload

for afl_dir in "$afl_root"/*; do
	[ -d "$afl_dir" ] || continue
	target_name=$(basename "$afl_dir")
	target_casr_dir="$casr_root/$target_name"
	rm -rf "$target_casr_dir"
	mkdir -p "$target_casr_dir"

	if ! find "$afl_dir" -path '*/crashes/id:*' -type f -print | grep -q .; then
		printf 'No AFL++ crashes found for %s.\n' "$target_name" > "$target_casr_dir/NO_CRASHES.txt"
		continue
	fi

	casr-afl --force-remove --hint san --timeout "$casr_timeout" \
		--ignore-cmdline -i "$afl_dir" -o "$target_casr_dir" \
		-- ./fuzz/fuzz_rsyslog_parsers @@
done
