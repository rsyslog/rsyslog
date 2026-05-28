#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
usage: devtools/check-test-antipatterns.sh [path ...]

Advisory scan for testbench patterns that have caused flakes in past CI runs.
The scanner prefers ripgrep when available and falls back to find + grep.

Findings are review prompts, not automatic failures. A match can be acceptable
when the test header documents why the pattern is intentional and what oracle
keeps the test deterministic.
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
	usage
	exit 0
fi

tmpfiles="$(mktemp)"
cleanup() {
	rm -f "$tmpfiles"
}
trap cleanup EXIT

add_shell_files() {
	local path
	for path in "$@"; do
		if [ -d "$path" ]; then
			find "$path" -type f -name '*.sh' -print
		elif [ -f "$path" ]; then
			case "$path" in
			*.sh) printf '%s\n' "$path" ;;
			esac
		fi
	done
}

if [ "$#" -eq 0 ]; then
	add_shell_files tests > "$tmpfiles"
else
	add_shell_files "$@" > "$tmpfiles"
fi

sort -u "$tmpfiles" -o "$tmpfiles"

if [ ! -s "$tmpfiles" ]; then
	printf 'No shell tests found.\n'
	exit 0
fi

print_matches() {
	local title="$1"
	local pattern="$2"
	local rationale="$3"
	local matches

	printf '\n## %s\n\n%s\n\n' "$title" "$rationale"
	if command -v rg >/dev/null 2>&1; then
		matches="$(xargs rg --line-number --with-filename --no-heading --color=never \
			--regexp "$pattern" < "$tmpfiles" || true)"
	else
		matches="$(xargs grep -nH -E -- "$pattern" < "$tmpfiles" || true)"
	fi

	if [ -n "$matches" ]; then
		printf '```text\n%s\n```\n' "$matches"
		return 0
	fi

	printf 'No matches.\n'
	return 1
}

findings=0

printf '# rsyslog test antipattern scan\n'
printf '\nScanned %s shell test files.\n' "$(wc -l < "$tmpfiles")"

if print_matches "Port preselection" \
	'get_free_port|get_free_port[[:space:]]' \
	'Preselecting a free port is inherently racy: another process can bind it before the test listener does. Prefer listener port="0" plus a port file, or a helper that writes readiness only after listen(2) succeeds.'; then
	findings=$((findings + 1))
fi

if print_matches "Fixed sleeps as synchronization" \
	'(^|[[:space:];])m?sleep[[:space:]][0-9]' \
	'Fixed sleeps often encode timing assumptions. Prefer explicit readiness or completion oracles such as port files, wait_file_lines, wait_queueempty, stats counters, or imdiag waits. If a sleep intentionally creates retry pressure, document that in the test header.'; then
	findings=$((findings + 1))
fi

if print_matches "Fixed TCP/UDP port literals" \
	'(port|PORT)[_[:alnum:]]*="?[1-9][0-9]{3,4}"?|:[1-9][0-9]{3,4}' \
	'Fixed port literals can collide with concurrently running tests or host services. Prefer dynamic listener port allocation with a port file. Review matches manually; some are input data, expected messages, or legacy constants.'; then
	findings=$((findings + 1))
fi

if print_matches "Backgrounded helpers" \
	'^[[:space:]]*[^#[:space:]].*[[:space:]]&([[:space:]]*(#.*)?)?$' \
	'Background processes need deterministic readiness and cleanup. Prefer testbench helpers that write readiness files after the service is actually ready, and ensure exit_test or explicit cleanup owns the process lifecycle.'; then
	findings=$((findings + 1))
fi

if print_matches "CPU/runtime thresholds" \
	'CPU ticks|threshold|TB_TEST_MAX_RUNTIME|TEST_MAX_RUNTIME|STARTUP_MAX_RUNTIME|TIMEOUT|timeout[[:space:]]+[0-9]' \
	'Timeouts and CPU/runtime thresholds are acceptable only with a clear oracle and rationale. For timing, sampling, retry, concurrency, or negative-path tests, the test header should explain what the threshold proves and why it is large enough under CI stress.'; then
	findings=$((findings + 1))
fi

if print_matches "Ad-hoc content checks" \
	'grep[[:space:]].*(error_exit|exit[[:space:]]+[0-9])|cat[[:space:]].*error_exit' \
	'Ad-hoc grep/cat assertions often produce inconsistent diagnostics or miss synchronization. Prefer content_check, custom_content_check, check_not_present, cmp_exact, and related diag.sh helpers unless the test needs custom logic.'; then
	findings=$((findings + 1))
fi

printf '\nSummary: %d antipattern classes had matches.\n' "$findings"
printf 'This advisory scan exits successfully so it can be used during review without blocking unrelated work.\n'
