#!/usr/bin/env bash
# Codex pre-push hook: Enforces full local container testing (Tier 1) per AGENTS.md.
# Automatically checks if C/H or test files were changed since the last container run.
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

def is_git_push(words: list[str]) -> bool:
    words = strip_prefixes(words)
    if not words or words[0] != "git":
        return False
    i = 1
    while i < len(words):
        token = words[i]
        if token == "push":
            return True
        if token == "--":
            return False
        if token in {"-c", "-C", "--git-dir", "--work-tree"}:
            i += 2
            continue
        if token.startswith("-"):
            i += 1
            continue
        return False
    return False

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

def contains_git_push(words: list[str]) -> bool:
    if is_git_push(words):
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

    return any(contains_git_push(simple_command) for simple_command in commands)

payload_raw = os.environ.get("PAYLOAD")
if payload_raw is None:
    sys.exit(0)
try:
    payload = json.loads(payload_raw)
except json.JSONDecodeError:
    sys.exit(0)
if payload.get("hook_event_name") != "PreToolUse":
    sys.exit(0)
if payload.get("tool_name") not in {"Bash", "run_command"}:
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
    if contains_git_push(simple_command):
        print("yes")
        break
PY
)"

if [[ "${should_gate}" != "yes" ]]; then
  exit 0
fi

# We are pushing! Let's check container validation.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
cd "${repo_root}"

# Check if SKIP_CONTAINER_VALIDATION is set
if [[ "${SKIP_CONTAINER_VALIDATION:-0}" == "1" ]]; then
  exit 0
fi

# A file `.codex/container_validated.marker` is updated when container tests pass.
marker_file=".codex/container_validated.marker"

if [[ ! -f "${marker_file}" ]]; then
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: Full local container validation has not been run or completed successfully. Per AGENTS.md, you MUST run full local container validation (Tier 1) before pushing. Run your container validation or, if unavailable, export SKIP_CONTAINER_VALIDATION=1 and explain the blocker."
  }
}
EOF
  exit 0
fi

# Check if source, build, or test files changed in the commits since the marker
marker_commit="$(xargs < "${marker_file}" || true)"

if [[ -z "${marker_commit}" ]] || ! git rev-parse --verify "${marker_commit}^{commit}" >/dev/null 2>&1; then
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: The validation marker is empty or invalid. Please re-run the container validation to update the marker."
  }
}
EOF
  exit 0
fi

# Use git diff to see if any source, test, or docker files have been modified since the validation marker
changed_since_validation="$(git diff --name-only "${marker_commit}" HEAD | grep -E '\.(c|h|sh|py)$|Makefile\.am|configure\.ac|Dockerfile|MODULE_METADATA\.yaml|^tests/' || true)"
if [[ -n "${changed_since_validation}" ]]; then
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: Changes to source, build, or test files have been made since the last local container validation run. Please run the local container validation (Tier 1) again on the updated code before pushing."
  }
}
EOF
  exit 0
fi

exit 0
