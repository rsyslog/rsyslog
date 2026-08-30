#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
usage: devtools/check-test-antipatterns.sh [path ...]

Advisory scan for testbench patterns that have caused flakes in past CI runs.
The scanner prefers ripgrep when available and falls back to find + grep.
Shell-specific checks inspect shell tests; the direct exit() check also covers
C and C++ test sources and helpers.

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
tmpsources="$(mktemp)"
cleanup() {
	rm -f "$tmpfiles" "$tmpsources"
}
trap cleanup EXIT

add_test_files() {
	local path
	for path in "$@"; do
		if [ -d "$path" ]; then
			find "$path" -type f \( -name '*.sh' -o -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' \) -print
		elif [ -f "$path" ]; then
			case "$path" in
			*.sh | *.c | *.cc | *.cpp | *.h) printf '%s\n' "$path" ;;
			esac
		fi
	done
}

if [ "$#" -eq 0 ]; then
	add_test_files tests > "$tmpsources"
else
	add_test_files "$@" > "$tmpsources"
fi

sort -u "$tmpsources" -o "$tmpsources"
sed -n '/\.sh$/p' "$tmpsources" > "$tmpfiles"

if [ ! -s "$tmpsources" ]; then
	printf 'No test sources found.\n'
	exit 0
fi

print_matches_in() {
	local title="$1"
	local pattern="$2"
	local rationale="$3"
	local filelist="$4"
	local matches

	printf '\n## %s\n\n%s\n\n' "$title" "$rationale"
	if [ ! -s "$filelist" ]; then
		printf 'No matches.\n'
		return 1
	fi
	if command -v rg >/dev/null 2>&1; then
		matches="$(xargs rg --line-number --with-filename --no-heading --color=never \
			--regexp "$pattern" < "$filelist" || true)"
	else
		matches="$(xargs grep -nH -E -- "$pattern" < "$filelist" || true)"
	fi

	if [ -n "$matches" ]; then
		printf '```text\n%s\n```\n' "$matches"
		return 0
	fi

	printf 'No matches.\n'
	return 1
}

print_direct_exit_matches() {
	local matches

	printf '\n## Direct exit() calls\n\n'
	printf '%s\n\n' \
		'Direct exit() calls are never acceptable in tests or test helpers because they bypass normal cleanup and can strand locks or worker threads. Return failure to the owning test thread and assert it after the lifecycle join. The testbench exit_test helper is not a direct exit() call.'
	matches="$(python3 - "$tmpsources" <<'PY'
import pathlib
import re
import sys


def strip_literals_and_comments(source):
    result = []
    state = "code"
    index = 0
    while index < len(source):
        char = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "/":
                result.extend("  ")
                index += 2
                state = "line_comment"
                continue
            if char == "/" and nxt == "*":
                result.extend("  ")
                index += 2
                state = "block_comment"
                continue
            if char == '"':
                result.append(" ")
                state = "string"
            elif char == "'":
                result.append(" ")
                state = "character"
            else:
                result.append(char)
        elif state == "line_comment":
            result.append("\n" if char == "\n" else " ")
            if char == "\n":
                state = "code"
        elif state == "block_comment":
            if char == "*" and nxt == "/":
                result.extend("  ")
                index += 2
                state = "code"
                continue
            result.append("\n" if char == "\n" else " ")
        else:
            if char == "\\" and nxt:
                result.append(" ")
                result.append("\n" if nxt == "\n" else " ")
                index += 2
                continue
            result.append("\n" if char == "\n" else " ")
            if (state == "string" and char == '"') or (state == "character" and char == "'"):
                state = "code"
        index += 1
    return "".join(result)


source_list = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
pattern = re.compile(r"(?<![A-Za-z0-9_.])exit\s*\(")
for filename in source_list:
    if pathlib.Path(filename).suffix not in {".c", ".cc", ".cpp", ".h"}:
        continue
    original = pathlib.Path(filename).read_text(encoding="utf-8", errors="replace")
    stripped = strip_literals_and_comments(original)
    original_lines = original.splitlines()
    for match in pattern.finditer(stripped):
        line_number = stripped.count("\n", 0, match.start()) + 1
        print(f"{filename}:{line_number}:{original_lines[line_number - 1].strip()}")
PY
)"
	if [ -n "$matches" ]; then
		printf '```text\n%s\n```\n' "$matches"
		return 0
	fi
	printf 'No matches.\n'
	return 1
}

print_matches() {
	print_matches_in "$1" "$2" "$3" "$tmpfiles"
}

findings=0

printf '# rsyslog test antipattern scan\n'
printf '\nScanned %s test source files (%s shell tests).\n' "$(wc -l < "$tmpsources")" "$(wc -l < "$tmpfiles")"

if print_direct_exit_matches; then
	findings=$((findings + 1))
fi

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
