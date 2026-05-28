#!/bin/sh
#
# Local validation dispatcher for AI and human-assisted rsyslog development.
#
# The helper has two modes:
#   - default: classify the current diff and print the validation plan
#   - --run: execute that plan and stop at the first required failure
#
# The changed-file set intentionally combines committed branch changes,
# staged/unstaged tracked changes, and untracked files. This avoids the common
# local-testing mistake where an agent validates only HEAD..base while the real
# patch still has uncommitted edits.
#
# Keep this script portable POSIX sh. It is meant to run on lean developer and
# container images, including Alpine-style environments where Bash may be
# absent. Optional diff-scoped tools such as shellcheck, checkbashisms, Cubic,
# and pycodestyle-backed formatters must not make this helper fail merely
# because they are unavailable; print a warning and continue. A validation tool
# that exists but reports a real finding still fails normally. Do not run
# full-tree shell portability scans here: they are slower and noisy for
# Bash-owned testbench scripts.
set -eu

mode=plan
base_ref="${RSYSLOG_LOCAL_VALIDATION_BASE:-origin/main}"

usage() {
	cat <<'EOF'
usage: devtools/local-validation-plan.sh [--run] [--base REF]

Print, or with --run execute, the local validation workflow for the current
diff. The changed-file set includes committed branch changes, staged and
unstaged tracked changes, and untracked files. Generated build products should
be cleaned before using this helper.

Environment knobs:
  RSYSLOG_LOCAL_VALIDATION_BASE  Base ref for committed branch changes (default: origin/main)
  RSYSLOG_LOCAL_BUILD_JOBS       Local build concurrency (default: 10)
  RSYSLOG_LOCAL_CHECK_JOBS       Local make check concurrency (default: 10)
  RSYSLOG_LOCAL_DOC_JOBS         Local Sphinx doc build jobs (default: nproc)

The --run mode exits on the first validation finding from a tool that actually
runs. Missing local tools produce warnings, not failures; hosted CI or a fuller
local environment must cover the skipped plumbing.
EOF
}

while [ "$#" -gt 0 ]; do
	case "$1" in
	--run)
		mode=run
		shift
		;;
	--base)
		if [ "$#" -lt 2 ]; then
			echo "error: --base requires an argument" >&2
			exit 2
		fi
		base_ref="$2"
		shift 2
		;;
	-h | --help)
		usage
		exit 0
		;;
	*)
		echo "error: unknown argument: $1" >&2
		usage >&2
		exit 2
		;;
	esac
done

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

tmp_changed="$(mktemp)"
tmp_shell="$(mktemp)"
tmp_python="$(mktemp)"
tmp_c="$(mktemp)"
cleanup() {
	rm -f "$tmp_changed" "$tmp_shell" "$tmp_python" "$tmp_c"
}
trap cleanup EXIT

if ! git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
	echo "error: base ref '$base_ref' is not available; fetch it or pass --base" >&2
	exit 2
fi

{
	git diff --name-only --diff-filter=d "$base_ref"...HEAD
	git diff --name-only --diff-filter=d HEAD
	git ls-files --others --exclude-standard
} | sort -u > "$tmp_changed"

if [ ! -s "$tmp_changed" ]; then
	echo "No local changes detected relative to $base_ref."
	exit 0
fi

matches_any() {
	pattern="$1"
	grep -Eq "$pattern" "$tmp_changed"
}

matches_only() {
	pattern="$1"
	! grep -Evq "$pattern" "$tmp_changed"
}

has_rendered_docs=0
has_workflow=0
has_build=0
has_testbench_plumbing=0
has_test_shell_only=0
has_c=0
has_runtime_ci=0

if matches_any '^(doc/source/|doc/Makefile\.am$|doc/tools/|doc/requirements\.txt$|doc/source/conf\.py$)'; then
	has_rendered_docs=1
fi
if matches_any '^\.github/workflows/'; then
	has_workflow=1
fi
if matches_any '(^|/)Makefile\.am$|^configure\.ac$|^m4/'; then
	has_build=1
fi
if matches_any '^tests/(diag\.sh|Makefile\.am|[^/]+\.[ch]$|helpers/|unit/)'; then
	has_testbench_plumbing=1
fi
if matches_only '^tests/[^/]+\.sh$'; then
	has_test_shell_only=1
fi
if matches_any '\.(c|h)$'; then
	has_c=1
fi

if matches_any '^((runtime|grammar|tools|plugins|contrib|compat)/|tests/|[^/]+\.(c|h)$|configure\.ac$|Makefile\.am$|m4/|\.github/workflows/run_checks\.yml$)'; then
	has_runtime_ci=1
fi

agent_docs_only=0
if matches_only '(^|/)AGENTS(\.local)?\.md$|^\.agent/skills/|^\.codex/skills/'; then
	agent_docs_only=1
fi

internal_docs_only=0
if [ "$has_rendered_docs" -eq 0 ] && matches_only '(^|/)AGENTS(\.local)?\.md$|^\.agent/skills/|^\.codex/skills/|^doc/ai/|^doc/security/|^doc/README\.md$|^doc/AGENTS\.md$|^doc/BUILDS_README\.md$|^doc/STRATEGY\.md$|^doc/IMPLEMENTATION_PLAN\.md$'; then
	internal_docs_only=1
fi

validation_tooling_only=0
if [ "$has_rendered_docs" -eq 0 ] && matches_only '(^|/)AGENTS(\.local)?\.md$|^\.agent/skills/|^\.codex/skills/|^doc/ai/|^doc/security/|^doc/README\.md$|^doc/AGENTS\.md$|^doc/BUILDS_README\.md$|^doc/STRATEGY\.md$|^doc/IMPLEMENTATION_PLAN\.md$|^devtools/(local-validation-plan\.sh|check-test-antipatterns\.sh|format-code\.sh|format-python\.sh|list-git-changed-c-h-files\.sh)$'; then
	validation_tooling_only=1
fi

grep -E '\.sh$' "$tmp_changed" > "$tmp_shell" || true
grep -E '\.py$' "$tmp_changed" > "$tmp_python" || true
grep -E '\.(c|h)$' "$tmp_changed" > "$tmp_c" || true

classification=general
if [ "$agent_docs_only" -eq 1 ]; then
	classification=agent-doc-only
elif [ "$internal_docs_only" -eq 1 ]; then
	classification=internal-doc-only
elif [ "$validation_tooling_only" -eq 1 ]; then
	classification=local-validation-tooling
elif [ "$has_rendered_docs" -eq 1 ] && [ "$has_runtime_ci" -eq 0 ] && [ "$has_workflow" -eq 0 ] && [ "$has_build" -eq 0 ]; then
	classification=rendered-docs
elif [ "$has_test_shell_only" -eq 1 ]; then
	classification=test-shell-only
elif [ "$has_testbench_plumbing" -eq 1 ]; then
	classification=testbench-plumbing
elif [ "$has_c" -eq 1 ] || [ "$has_build" -eq 1 ] || [ "$has_workflow" -eq 1 ] || [ "$has_runtime_ci" -eq 1 ]; then
	classification=code-or-ci
fi

nproc_jobs() {
	if command -v nproc >/dev/null 2>&1; then
		nproc
	else
		getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1
	fi
}

doc_jobs="${RSYSLOG_LOCAL_DOC_JOBS:-$(nproc_jobs)}"
build_jobs="${RSYSLOG_LOCAL_BUILD_JOBS:-10}"
check_jobs="${RSYSLOG_LOCAL_CHECK_JOBS:-10}"

print_plan() {
	echo "Local validation classification: $classification"
	echo "Base ref: $base_ref"
	echo
	echo "Changed files:"
	sed 's/^/  /' "$tmp_changed"
	echo
	echo "Recommended validation:"
	case "$classification" in
	agent-doc-only | internal-doc-only)
		echo "  - Text review and command-snippet sanity checks as needed."
		echo "  - No runtime container CI, static analyzer, or Sphinx build required."
		;;
	local-validation-tooling)
		echo "  - Shell/Python style checks for changed tooling."
		echo "  - Dry-run this helper against representative diffs where practical."
		echo "  - No runtime container CI unless the tooling change affects CI/container execution."
		;;
	rendered-docs)
		echo "  - ./doc/tools/build-doc-linux.sh --clean --format html --jobs \"$doc_jobs\""
		echo "  - Add --strict for larger or structural documentation edits."
		;;
	test-shell-only)
		echo "  - shellcheck changed tests if shellcheck is installed."
		echo "  - Focused container test with CI_MAKE_CHECK_TESTS set to changed tests."
		echo "  - Add broad Ubuntu 26.04 run if the test changes timing, ports, process lifecycle, or shared behavior."
		;;
	testbench-plumbing | code-or-ci | general)
		echo "  - Available cheap diff-scoped linters."
		echo "  - Cubic where applicable."
		echo "  - Ubuntu 26.04 static analyzer for C/testbench/code changes."
		echo "  - Change-gated Ubuntu 26.04 run-ci.sh check."
		;;
	esac
}

run_shellcheck() {
	if [ ! -s "$tmp_shell" ]; then
		return 0
	fi
	if command -v shellcheck >/dev/null 2>&1; then
		while IFS= read -r file; do
			shellcheck -S warning "$file"
		done < "$tmp_shell"
	else
		echo "warning: shellcheck not installed; skipping shell lint" >&2
	fi
	if command -v checkbashisms >/dev/null 2>&1; then
		while IFS= read -r file; do
			read -r first_line < "$file" || first_line=
			case "$first_line" in
			'#!/bin/sh' | '#!/usr/bin/sh' | '#!/usr/bin/env sh')
				checkbashisms -p "$file"
				;;
			esac
		done < "$tmp_shell"
	else
		echo "warning: checkbashisms not installed; skipping POSIX shell portability lint" >&2
	fi
}

run_python_style() {
	if [ ! -s "$tmp_python" ]; then
		return 0
	fi
	if [ -x devtools/format-python.sh ]; then
		while IFS= read -r file; do
			devtools/format-python.sh --check-if-available "$file"
		done < "$tmp_python"
	else
		echo "warning: devtools/format-python.sh missing or not executable; skipping Python style check" >&2
	fi
}

run_format_code_check() {
	if [ ! -s "$tmp_c" ]; then
		return 0
	fi
	if [ -x devtools/format-code.sh ]; then
		devtools/format-code.sh --git-changed
	else
		echo "warning: devtools/format-code.sh missing or not executable; skipping C format check" >&2
	fi
}

run_docs_build() {
	if [ ! -x ./doc/tools/build-doc-linux.sh ]; then
		echo "warning: doc/tools/build-doc-linux.sh missing or not executable; skipping docs build" >&2
		echo "warning: this run is not docs-validated; use hosted CI or a fuller local environment" >&2
		return 0
	fi
	./doc/tools/build-doc-linux.sh --clean --format html --jobs "$doc_jobs"
}

have_container_tooling() {
	if ! command -v docker >/dev/null 2>&1; then
		echo "warning: docker not installed; skipping selected local container validation lane" >&2
		echo "warning: this run is not fully container-validated; use hosted CI or a container-capable agent" >&2
		return 1
	fi
	return 0
}

run_distclean_if_available() {
	if command -v make >/dev/null 2>&1; then
		make distclean || true
	else
		echo "warning: make not installed; skipping distclean before container lane" >&2
	fi
}

run_timed_env() {
	if [ -x /usr/bin/time ]; then
		/usr/bin/time -p env "$@"
	else
		echo "warning: /usr/bin/time missing; running without runtime measurement" >&2
		env "$@"
	fi
}

have_devcontainer_script() {
	if [ -x devtools/devcontainer.sh ]; then
		return 0
	fi
	echo "warning: devtools/devcontainer.sh missing or not executable; skipping container lane" >&2
	echo "warning: this run is not fully container-validated; use hosted CI or a fuller local environment" >&2
	return 1
}

run_cubic_if_available() {
	case "$classification" in
	agent-doc-only | internal-doc-only | rendered-docs)
		return 0
		;;
	esac
	if command -v cubic >/dev/null 2>&1; then
		cubic review --print-logs --base HEAD
	else
		echo "warning: cubic not installed; skipping local Cubic review" >&2
	fi
}

run_static_analyzer() {
	have_container_tooling || return 0
	have_devcontainer_script || return 0
	run_distclean_if_available
	rm -rf scan-build-report clang-analyzer.log
	run_timed_env \
		RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
		SCAN_BUILD='scan-build' \
		SCAN_BUILD_CC='clang' \
		SCAN_BUILD_REPORT_DIR='scan-build-report' \
		CI_MAKE_OPT='-j20' \
		DOCKER_RUN_EXTRA_OPTS='-e SCAN_BUILD -e SCAN_BUILD_CC -e SCAN_BUILD_REPORT_DIR' \
		RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
		devtools/devcontainer.sh --rm devtools/run-static-analyzer.sh
}

run_change_gated_ubuntu26() {
	have_container_tooling || return 0
	have_devcontainer_script || return 0
	if ! command -v bash >/dev/null 2>&1; then
		echo "warning: bash not installed; skipping service relevance setup and container lane" >&2
		echo "warning: this run is not fully container-validated; use hosted CI or a fuller local environment" >&2
		return 0
	fi
	run_distclean_if_available
	export RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04'
	export RSYSLOG_TESTBENCH_CHANGED_FILES
	RSYSLOG_TESTBENCH_CHANGED_FILES="$(cat "$tmp_changed")"
	export CC='gcc'
	export CFLAGS='-g'
	export CI_CONFIGURE_CACHE=1
	export CI_MAKE_OPT="-j${build_jobs}"
	export CI_MAKE_CHECK_OPT="-j${check_jobs}"
	export CI_CHECK_CMD='check'
	export VERBOSE=1
	run_timed_env bash -c \
		'. devtools/apply-service-relevance.sh && rsyslog_apply_default_pr_service_suppressions && exec devtools/devcontainer.sh --rm devtools/run-ci.sh'
}

run_focused_test_shell() {
	tests="$(grep -E '^tests/[^/]+\.sh$' "$tmp_changed" | sed 's#^tests/##' | tr '\n' ' ')"
	if [ -z "$tests" ]; then
		return 0
	fi
	have_container_tooling || return 0
	have_devcontainer_script || return 0
	run_distclean_if_available
	export RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04'
	export CC='gcc'
	export CFLAGS='-g'
	export CI_CONFIGURE_CACHE=1
	export CI_MAKE_OPT="-j${build_jobs}"
	export CI_MAKE_CHECK_OPT="-j${check_jobs}"
	export CI_MAKE_CHECK_TESTS="$tests"
	export CI_CHECK_CMD='check'
	export VERBOSE=1
	run_timed_env devtools/devcontainer.sh --rm devtools/run-ci.sh
}

print_plan

if [ "$mode" = plan ]; then
	exit 0
fi

echo
echo "Executing local validation plan..."
run_shellcheck
run_python_style
run_format_code_check
git diff --check "$base_ref"...HEAD
git diff --cached --check
git diff --check

case "$classification" in
agent-doc-only | internal-doc-only)
	echo "Validation complete: documentation/instruction-only change."
	;;
local-validation-tooling)
	echo "Validation complete: local validation tooling checks passed."
	;;
rendered-docs)
	run_docs_build
	;;
test-shell-only)
	run_focused_test_shell
	;;
testbench-plumbing | code-or-ci | general)
	run_cubic_if_available
	run_static_analyzer
	run_change_gated_ubuntu26
	;;
esac
