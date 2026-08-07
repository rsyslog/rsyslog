#!/bin/sh
# Smoke-test the opt-in imtcp session fuzzer against framing and compression
# seeds. A clean fixed iteration run is the oracle; any crash, sanitizer
# report, timeout, or harness invariant failure makes libFuzzer return non-zero.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
fuzzer="${abs_top_builddir:-"$test_dir/.."}/tools/fuzz_imtcp_session"
seed_source="${srcdir:-"$test_dir"}/fuzz/corpus/imtcp-session"
prepare_corpus="${srcdir:-"$test_dir"}/fuzz/prepare-imtcp-session-corpus.py"
test -x "$fuzzer" || exit 77

work_corpus=$(mktemp -d "$PWD/fuzz-imtcp-session-work.XXXXXX")
cleanup_work_corpus() {
	rc=$?
	if test "$rc" -eq 0; then
		rm -rf -- "$work_corpus"
	else
		printf 'preserving failed imtcp fuzz corpus: %s\n' "$work_corpus" >&2
	fi
}
trap cleanup_work_corpus 0

python3 "$prepare_corpus" "$seed_source" "$work_corpus"
"$fuzzer" \
	-runs=1000 \
	-max_len=65536 \
	-timeout=5 \
	-rss_limit_mb=2048 \
	-artifact_prefix="$work_corpus/artifact-" \
	"$work_corpus"
