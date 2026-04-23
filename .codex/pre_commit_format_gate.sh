#!/usr/bin/env bash
set -euo pipefail

payload="$(cat)"

if [[ -z "${payload}" ]]; then
  exit 0
fi

should_gate="$(
  PAYLOAD="$payload" python3 <<'PY'
import json
import os
import posixpath
import re
import shlex
import sys


def split_simple_commands(command: str) -> list[list[str]]:
    lexer = shlex.shlex(command, posix=True, punctuation_chars=";&|()")
    lexer.whitespace_split = True
    lexer.commenters = ""

    commands = []
    current = []

    for token in lexer:
        if re.fullmatch(r"[;&|()]+", token):
            if current:
                commands.append(current)
                current = []
            continue
        current.append(token)

    if current:
        commands.append(current)

    return commands


def strip_prefixes(words: list[str]) -> list[str]:
    i = 0
    assignment_re = re.compile(r"[A-Za-z_][A-Za-z0-9_]*=.*")

    while i < len(words):
        token = words[i]

        if token == "sudo":
            i += 1
            continue

        if token in {"command", "builtin", "noglob", "time"}:
            i += 1
            continue

        if token == "env":
            i += 1
            while i < len(words) and assignment_re.fullmatch(words[i]):
                i += 1
            continue

        if assignment_re.fullmatch(token):
            i += 1
            continue

        break

    return words[i:]


def unwrap_shell_command(words: list[str]) -> list[str] | None:
    words = strip_prefixes(words)
    if not words:
        return None

    shell_name = posixpath.basename(words[0])
    if shell_name not in {"bash", "sh", "zsh"}:
        return None

    i = 1
    while i < len(words):
        token = words[i]

        if token == "--":
            i += 1
            break

        if token.startswith("-") and "c" in token[1:]:
            if i + 1 >= len(words):
                return None
            return words[i + 1 :]

        if token.startswith("-"):
            i += 1
            continue

        return None

    return None


def is_git_commit(words: list[str]) -> bool:
    words = strip_prefixes(words)
    if not words or words[0] != "git":
        return False

    i = 1
    while i < len(words):
        token = words[i]

        if token == "commit":
            return True

        if token == "--":
            return False

        if token in {
            "-c",
            "-C",
            "--git-dir",
            "--work-tree",
            "--namespace",
            "--super-prefix",
            "--config-env",
        }:
            i += 2
            continue

        if token.startswith("--git-dir=") or token.startswith("--work-tree=") \
                or token.startswith("--namespace=") or token.startswith("--super-prefix=") \
                or token.startswith("--config-env="):
            i += 1
            continue

        if token.startswith("-"):
            i += 1
            continue

        return False

    return False


def contains_git_commit(words: list[str]) -> bool:
    if is_git_commit(words):
        return True

    nested_command = unwrap_shell_command(words)
    if not nested_command:
        return False

    command = nested_command[0]
    if not isinstance(command, str) or not command.strip():
        return False

    try:
        commands = split_simple_commands(command)
    except ValueError:
        return False

    return any(contains_git_commit(simple_command) for simple_command in commands)


payload_raw = os.environ.get("PAYLOAD")
if payload_raw is None:
    sys.exit(0)

try:
    payload = json.loads(payload_raw)
except json.JSONDecodeError:
    sys.exit(0)

if payload.get("hook_event_name") != "PreToolUse":
    sys.exit(0)

if payload.get("tool_name") != "Bash":
    sys.exit(0)

tool_input = payload.get("tool_input") or {}
command = tool_input.get("command")
if not isinstance(command, str) or not command.strip():
    sys.exit(0)

try:
    commands = split_simple_commands(command)
except ValueError:
    sys.exit(0)

for simple_command in commands:
    if contains_git_commit(simple_command):
        print("yes")
        break
PY
)"

if [[ "${should_gate}" != "yes" ]]; then
  exit 0
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

cd "${repo_root}"

tracked_changed_c_h_files() {
  ./devtools/list-git-changed-c-h-files.sh
}

snapshot_tracked_changed_c_h_files() {
  local tracked_files

  mapfile -t tracked_files < <(tracked_changed_c_h_files)

  if [[ "${#tracked_files[@]}" -eq 0 ]]; then
    printf '[]'
    return
  fi

  python3 - "${tracked_files[@]}" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

entries = []
for raw_path in sys.argv[1:]:
    path = Path(raw_path)
    if not path.is_file():
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    entries.append([path.as_posix(), digest])

print(json.dumps(entries, separators=(",", ":")))
PY
}

run_repo_policy_focus_check() {
  local tmpdir base_ref snapshot_commit applicable_count

  if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
    return 0
  fi

  if ! command -v python3 >/dev/null 2>&1; then
    return 0
  fi

  tmpdir="$(mktemp -d)"
  cleanup_repo_policy_focus_check() {
    rm -rf -- "${tmpdir}"
  }
  trap cleanup_repo_policy_focus_check RETURN

  base_ref="$(git rev-parse HEAD)"
  snapshot_commit="$(
    printf 'codex repo-policy focus snapshot\n' | git commit-tree "$(git write-tree)" -p "${base_ref}"
  )"

  python3 scripts/build_repo_policy_focus_input.py \
    --base "${base_ref}" \
    --head "${snapshot_commit}" \
    --output "${tmpdir}/review-package.json"

  applicable_count="$(
    python3 - "${tmpdir}/review-package.json" <<'PY'
import json
import sys
from pathlib import Path

package = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
print(int(package.get("applicable_count", 0)))
PY
  )"

  if [[ "${applicable_count}" == "0" ]]; then
    return 0
  fi

  python3 scripts/evaluate_repo_policy_focus.py \
    --input "${tmpdir}/review-package.json" \
    --output "${tmpdir}/model-output.json"

  python3 scripts/score_repo_policy_focus.py \
    --review-input "${tmpdir}/review-package.json" \
    --model-output "${tmpdir}/model-output.json" \
    --summary-md "${tmpdir}/summary.md" \
    --summary-json "${tmpdir}/summary.json"

  python3 - "${tmpdir}/summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
warnings = [check for check in summary["checks"] if check["status"] == "warn"]
failures = [check for check in summary["checks"] if check["status"] == "fail"]

if warnings:
    print("Focused repo-policy warnings:", file=sys.stderr)
    for check in warnings:
        print(f"- {check['id']}: {check['reason']}", file=sys.stderr)
        for issue in check["issues"]:
            location = issue["file"] or "(no file)"
            if issue["line"]:
                location = f"{location}:{issue['line']}"
            print(f"  * {location} - {issue['message']}", file=sys.stderr)

if not failures:
    raise SystemExit(0)

print("Focused repo-policy checks failed:", file=sys.stderr)
for check in failures:
    print(f"- {check['id']}: {check['reason']}", file=sys.stderr)
    for issue in check["issues"]:
        location = issue["file"] or "(no file)"
        if issue["line"]:
            location = f"{location}:{issue['line']}"
        print(f"  * {location} - {issue['message']}", file=sys.stderr)
raise SystemExit(1)
PY
}

if ! run_repo_policy_focus_check; then
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Deterministic repo-policy checks failed. Fix the reported invariant violations before running git commit again."
  }
}
EOF
  exit 0
fi

if ! command -v clang-format-18 >/dev/null 2>&1; then
  exit 0
fi

git_c_h_change_state="$(
  python3 <<'PY'
import subprocess


def listed_diff(*args: str) -> set[str]:
    proc = subprocess.run(
        ["git", "diff", *args, "--diff-filter=d", "--name-only", "--", "*.c", "*.h"],
        check=True,
        text=True,
        capture_output=True,
    )
    return {line for line in proc.stdout.splitlines() if line}


unstaged = listed_diff()
staged = listed_diff("--cached")

print("changed=yes" if (unstaged or staged) else "changed=no")
for path in sorted(unstaged & staged):
    print(path)
PY
)"

if [[ "${git_c_h_change_state}" == changed=no* ]]; then
  exit 0
fi

partially_staged_c_h_files="$(printf '%s\n' "${git_c_h_change_state}" | sed '1d')"

if [[ -n "${partially_staged_c_h_files}" ]]; then
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Commit blocked: partially staged .c/.h files are present. The formatter hook cannot safely restage those files automatically."
  }
}
EOF
  exit 0
fi

mapfile -t initially_staged_c_h_files < <(
  git diff --cached --diff-filter=d --name-only -- '*.c' '*.h'
)

snapshot_files="$(snapshot_tracked_changed_c_h_files)"

if ./devtools/format-code.sh --git-changed >&2; then
  snapshot_after="$(snapshot_tracked_changed_c_h_files)"

  if [[ "${snapshot_files}" == "${snapshot_after}" ]]; then
    exit 0
  fi

  if [[ "${#initially_staged_c_h_files[@]}" -gt 0 ]]; then
    git add -- "${initially_staged_c_h_files[@]}"
  fi
  exit 0
fi

cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Formatting failed: ./devtools/format-code.sh exited non-zero. Fix formatting before running git commit."
  }
}
EOF
