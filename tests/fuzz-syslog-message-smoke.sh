#!/bin/sh
# Smoke-test the opt-in syslog parser fuzzer against its seed corpus. A clean
# fixed iteration run is the oracle; any crash, sanitizer report, or invariant
# failure makes libFuzzer return non-zero.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
fuzzer="${abs_top_builddir:-"$test_dir/.."}/tools/fuzz_rsyslog_message"
corpus="${srcdir:-"$test_dir"}/fuzz/corpus/syslog-message"
test -x "$fuzzer" || exit 77

work_corpus="./fuzz-syslog-message-work.$$"
mkdir "$work_corpus"
cleanup_work_corpus() {
	rc=$?
	if test "$rc" -eq 0; then
		rm -rf -- "$work_corpus"
	fi
}
trap cleanup_work_corpus 0

"$fuzzer" \
	-runs=1000 \
	-max_len=65536 \
	-timeout=5 \
	-rss_limit_mb=2048 \
	-artifact_prefix="$work_corpus/artifact-" \
	"$work_corpus" \
	"$corpus"
