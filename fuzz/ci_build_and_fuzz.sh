#!/bin/sh
## ci_build_and_fuzz.sh
## Build the AFL++ parser target and run the GitLab fuzzing campaign.

set -eu

repo_dir=${CI_PROJECT_DIR:-/src}
cd "$repo_dir"

. "$repo_dir/fuzz/ci_fuzz_common.sh"

target_name=${FUZZ_TARGET_NAME:-rsyslog_parsers}
seed_dir=${FUZZ_SEED_DIR:-fuzz/testcases/test_inputs}
out_dir=${FUZZ_OUT_DIR:-artifacts/out/$target_name}
evidence_dir=${FUZZ_EVIDENCE_DIR:-artifacts/evidence/$target_name}
no_new_path_seconds=${FUZZ_NO_NEW_PATH_SECONDS:-7200}
max_total_time_seconds=${FUZZ_MAX_TOTAL_TIME_SECONDS:-7200}
timeout_ms=${FUZZ_TIMEOUT_MS:-1000+}

export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=${AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES:-1}
export AFL_IGNORE_PROBLEMS=${AFL_IGNORE_PROBLEMS:-1}
export AFL_MAP_SIZE=${AFL_MAP_SIZE:-2097152}
export AFL_NO_UI=${AFL_NO_UI:-1}
export AFL_QUIET=${AFL_QUIET:-1}
export AFL_SKIP_CPUFREQ=${AFL_SKIP_CPUFREQ:-1}
export AFL_EXIT_ON_TIME=${AFL_EXIT_ON_TIME:-$no_new_path_seconds}
export ASAN_OPTIONS=${AFL_ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1:symbolize=0}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}

ci_build_afl_target

mkdir -p "$evidence_dir" artifacts/metadata "${TMPDIR:-artifacts/tmp}"
export TMPDIR=${TMPDIR:-$repo_dir/artifacts/tmp}

make -C fuzz \
	FUZZ_COVERAGE=0 \
	FUZZ_SANITIZERS= \
	FUZZ_NO_SANITIZE_FLAGS=-fno-sanitize=function \
	check-fuzz-regressions > "$evidence_dir/regressions.log" 2>&1
cat "$evidence_dir/regressions.log"

FUZZ_TIMEOUT=${FUZZ_SMOKE_TIMEOUT:-15} \
	./fuzz/run_fuzz_smoke.sh file > "$evidence_dir/replay.log" 2>&1
cat "$evidence_dir/replay.log"

make -C fuzz validate-fuzz-dicts > "$evidence_dir/dictionaries.log" 2>&1
cat "$evidence_dir/dictionaries.log"

cp ./fuzz/fuzz_rsyslog_parsers "$evidence_dir/"
sha256sum "$evidence_dir/fuzz_rsyslog_parsers" > "$evidence_dir/SHA256SUMS"
git rev-parse HEAD > artifacts/metadata/rsyslog-revision.txt
git status --short > artifacts/metadata/git-status.txt
"$CC" --version > artifacts/metadata/compiler.txt
ldd ./fuzz/fuzz_rsyslog_parsers > artifacts/metadata/linked-libraries.txt

rm -rf "$out_dir"
mkdir -p "$out_dir"
ci_set_afl_preload

status=0
afl-fuzz -V "$max_total_time_seconds" -i "$seed_dir" -o "$out_dir" \
	-x fuzz/rsyslog.dict -m none -t "$timeout_ms" \
	-- ./fuzz/fuzz_rsyslog_parsers @@ || status=$?

case "$status" in
	0) ;;
	*) exit "$status" ;;
esac

mkdir -p "$out_dir/default/queue" "$out_dir/default/crashes" "$out_dir/default/hangs"
if [ -f "$out_dir/default/fuzzer_stats" ] && [ ! -f "$out_dir/fuzzer_stats" ]; then
	cp "$out_dir/default/fuzzer_stats" "$out_dir/fuzzer_stats"
fi
