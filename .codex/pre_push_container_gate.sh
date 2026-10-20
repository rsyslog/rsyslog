#!/usr/bin/env bash
# Codex pre-push hook: Enforces full local container testing (Tier 1) per AGENTS.md.
# Automatically checks if C/H or test files were changed since the last container run.
set -euo pipefail

payload="$(cat)"

if [[ -z "${payload}" ]]; then
  exit 0
fi

gate_selection="$(
  PAYLOAD="$payload" python3 <<'PY'
import json
import os
import posixpath
import re
import shlex
import sys

def split_simple_commands(command):
    lexer = shlex.shlex(command, posix=True, punctuation_chars=";&|()")
    # Keep the default lexer mode: Python 3.6 ignores punctuation characters
    # adjacent to a word when whitespace_split is enabled.
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

def strip_prefixes(words):
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
            while i < len(words):
                token = words[i]
                if token == "-u" and i + 1 < len(words):
                    i += 2
                    continue
                if token == "--unset" and i + 1 < len(words):
                    i += 2
                    continue
                if token.startswith("-") or assignment_re.fullmatch(token):
                    i += 1
                    continue
                break
            continue
        if assignment_re.fullmatch(token):
            i += 1
            continue
        break
    return words[i:]

def is_shell_directory_change(words):
    words = strip_prefixes(words)
    return bool(words and words[0] in {"cd", "pushd", "popd"})

def resolve_directory(path, current_directory):
    if os.path.isabs(path):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(current_directory, path))

def git_push_target(words, fallback_directory):
    """Return a push target, or an error for unsafe repository selection."""
    words = strip_prefixes(words)
    if not words or words[0] != "git":
        return None

    i = 1
    current_directory = fallback_directory
    explicit_target = False
    first_c_is_absolute = False
    ambiguous_selector = None
    while i < len(words):
        token = words[i]
        if token == "push":
            if ambiguous_selector:
                return {"status": "error", "reason": ambiguous_selector}
            return {"status": "push", "target": current_directory,
                    "safe_after_shell_directory_change":
                    explicit_target and first_c_is_absolute}
        if token == "--":
            return None
        if token == "-C":
            if i + 1 >= len(words):
                return {"status": "error", "reason": "git -C has no directory"}
            if not explicit_target:
                first_c_is_absolute = os.path.isabs(words[i + 1])
            current_directory = resolve_directory(words[i + 1], current_directory)
            explicit_target = True
            i += 2
            continue
        if token.startswith("-C") and token != "-C":
            if not explicit_target:
                first_c_is_absolute = os.path.isabs(token[2:])
            current_directory = resolve_directory(token[2:], current_directory)
            explicit_target = True
            i += 1
            continue
        if token in {"--git-dir", "--work-tree"} or token.startswith("--git-dir=") \
                or token.startswith("--work-tree="):
            ambiguous_selector = "git --git-dir/--work-tree cannot be mapped to one worktree"
            i += 2 if token in {"--git-dir", "--work-tree"} else 1
            continue
        if token == "-c":
            i += 2
            continue
        if token.startswith("-"):
            i += 1
            continue
        return None
    return False

def unwrap_shell_command(words):
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

def inline_validation_override(words):
    """Return the documented push override, or None when the command omits it."""
    assignment_re = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=(.*)")
    i = 0
    override = None
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
            while i < len(words):
                token = words[i]
                if token == "-u" and i + 1 < len(words):
                    if words[i + 1] == "SKIP_CONTAINER_VALIDATION":
                        override = "0"
                    i += 2
                    continue
                if token == "--unset" and i + 1 < len(words):
                    if words[i + 1] == "SKIP_CONTAINER_VALIDATION":
                        override = "0"
                    i += 2
                    continue
                if token in {"-uSKIP_CONTAINER_VALIDATION", "--unset=SKIP_CONTAINER_VALIDATION"}:
                    override = "0"
                    i += 1
                    continue
                if token.startswith("-"):
                    i += 1
                    continue
                break
            continue
        match = assignment_re.fullmatch(token)
        if match:
            if match.group(1) == "SKIP_CONTAINER_VALIDATION":
                override = match.group(2)
            i += 1
            continue
        break
    if override is None:
        return None
    return override == "1"

def shell_validation_reset(words):
    """Return whether a shell builtin clears the push override."""
    words = strip_prefixes(words)
    if not words:
        return False
    return words[0] == "unset" and "SKIP_CONTAINER_VALIDATION" in words[1:]

def classify_git_push(words, inherited_skip, fallback_directory):
    override = inline_validation_override(words)
    effective_skip = inherited_skip if override is None else override
    target = git_push_target(words, fallback_directory)
    if target:
        if target["status"] == "error":
            return [target]
        if effective_skip:
            return [{"status": "skip"}]
        return [target]

    nested_command = unwrap_shell_command(words)
    if not nested_command:
        return []

    command = nested_command[0]
    if not isinstance(command, str) or not command.strip():
        return []

    try:
        commands = split_simple_commands(command)
    except ValueError:
        return []

    decisions = []
    shell_directory_change = False
    nested_skip = effective_skip
    for simple_command in commands:
        if nested_skip and shell_validation_reset(simple_command):
            nested_skip = False
            continue
        shell_directory_change = shell_directory_change or is_shell_directory_change(simple_command)
        decisions.extend(classify_git_push(simple_command, nested_skip, fallback_directory))

    if shell_directory_change:
        for decision in decisions:
            if decision.get("status") == "push" and not decision["safe_after_shell_directory_change"]:
                return [{"status": "error", "reason":
                         "shell directory changes make a bare git push target ambiguous"}]
    return decisions

def classify_command_list(commands, fallback_directory, inherited_skip=False):
    decisions = []
    directory_change = False
    for simple_command in commands:
        if inherited_skip and shell_validation_reset(simple_command):
            inherited_skip = False
            continue
        directory_change = directory_change or is_shell_directory_change(simple_command)
        decisions.extend(classify_git_push(simple_command, inherited_skip, fallback_directory))

    if directory_change:
        for decision in decisions:
            if decision.get("status") == "push" and not decision["safe_after_shell_directory_change"]:
                return [{"status": "error", "reason":
                         "shell directory changes make a bare git push target ambiguous"}]
    return decisions

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

fallback_directory = tool_input.get("workdir")
if not isinstance(fallback_directory, str) or not fallback_directory:
    fallback_directory = payload.get("cwd")
if not isinstance(fallback_directory, str) or not fallback_directory:
    fallback_directory = os.getcwd()
fallback_directory = os.path.abspath(fallback_directory)

try:
    commands = split_simple_commands(command)
except ValueError:
    sys.exit(0)

decisions = classify_command_list(commands, fallback_directory,
                                  inherited_skip=os.environ.get("SKIP_CONTAINER_VALIDATION") == "1")
errors = [decision["reason"] for decision in decisions if decision.get("status") == "error"]
if errors:
    print(json.dumps({"error": errors[0]}))
    sys.exit(0)

targets = [decision["target"] for decision in decisions if decision.get("status") == "push"]
print(json.dumps({"targets": targets}))
PY
)"

selection_error="$(SELECTION="${gate_selection}" python3 <<'PY'
import json
import os

selection = json.loads(os.environ["SELECTION"])
print(selection.get("error", ""))
PY
)"

if [[ -n "${selection_error}" ]]; then
  cat <<EOF
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: ${selection_error}. Use git -C /absolute/worktree push so the validation gate can identify the target worktree."
  }
}
EOF
  exit 0
fi

mapfile -d '' -t gate_targets < <(
  SELECTION="${gate_selection}" python3 <<'PY'
import json
import os
import sys

selection = json.loads(os.environ["SELECTION"])
for target in selection.get("targets", []):
    sys.stdout.buffer.write(target.encode() + b"\0")
PY
)

if [[ "${#gate_targets[@]}" -eq 0 ]]; then
  exit 0
fi

# GitHub's run_checks.yml classifies its runtime lane from the PR delta. Keep
# this local gate aligned with that policy: documentation, README, AGENTS, and
# other non-runtime changes do not need a runtime-container validation marker.
# Retain the previous gate coverage for build and metadata inputs. Use the PR
# base, rather than the marker, so an old marker cannot make unrelated
# main-branch changes look like part of the local push.
is_non_runtime_push_delta() {
  local base_ref

  for base_ref in "${RSYSLOG_LOCAL_VALIDATION_BASE:-}" origin/main upstream/main '@{upstream}'; do
    [ -n "${base_ref}" ] || continue
    if git rev-parse --verify "${base_ref}^{commit}" >/dev/null 2>&1; then
      base_ref="$(git merge-base HEAD "${base_ref}")" || return 1
      break
    fi
    base_ref=''
  done

  [ -n "${base_ref:-}" ] || return 1

  local changed_files
  changed_files="$(git diff --name-only "${base_ref}"...HEAD)"
  [ -n "${changed_files}" ] || return 1
  ! grep -Ev '^doc/Makefile\.am$' <<<"${changed_files}" |
    grep -Eq '(^|/).*\.(c|h|sh|py)$|(^|/)Dockerfile|(^|/)MODULE_METADATA\.yaml$|^grammar/(lexer\.l|grammar\.y)$|^tests/[^/]+\.sh$|^diag\.sh$|(^|/)Makefile\.am$|^configure\.ac$|^\.github/workflows/run_checks\.yml$'
}

validate_target() {
  local requested_target="$1" repo_root marker_file marker_commit changed_since_validation

  if ! repo_root="$(git -C "${requested_target}" rev-parse --show-toplevel 2>/dev/null)"; then
    printf 'invalid-target\n'
    return 1
  fi

  cd "${repo_root}"

  if is_non_runtime_push_delta; then
    return 0
  fi

  marker_file=".codex/container_validated.marker"
  if [[ ! -f "${marker_file}" ]]; then
    printf 'missing-marker\n'
    return 1
  fi

  marker_commit="$(xargs < "${marker_file}" || true)"
  if [[ -z "${marker_commit}" ]] || ! git rev-parse --verify "${marker_commit}^{commit}" >/dev/null 2>&1; then
    printf 'invalid-marker\n'
    return 1
  fi

  changed_since_validation="$(git diff --name-only "${marker_commit}" HEAD | grep -E '\.(c|h|sh|py)$|Makefile\.am|configure\.ac|Dockerfile|MODULE_METADATA\.yaml|^tests/|^grammar/(lexer\.l|grammar\.y)$|^\.github/workflows/run_checks\.yml$' || true)"
  if [[ -n "${changed_since_validation}" ]]; then
    printf 'stale-marker\n'
    return 1
  fi
}

for gate_target in "${gate_targets[@]}"; do
  validation_result="$(validate_target "${gate_target}")" || case "${validation_result}" in
  invalid-target)
    cat <<EOF
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: the requested target is not a Git worktree. Use git -C /absolute/worktree push with the intended worktree."
  }
}
EOF
    exit 0
    ;;
  missing-marker)
  cat <<'EOF'
{
  "hookSpecificOutput": {
    "hookEventName": "PreToolUse",
    "permissionDecision": "deny",
    "permissionDecisionReason": "Push blocked: no local container validation marker exists for this worktree. Run devtools/record-container-validation.py after the required validation, including reviewed accepted rerun evidence when a bounded flake was reconciled. If container validation is unavailable, export SKIP_CONTAINER_VALIDATION=1 and explain the blocker."
  }
}
EOF
  exit 0
    ;;
  invalid-marker)
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
    ;;
  stale-marker)
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
    ;;
  *)
    printf 'Push blocked: unable to validate %s.\n' "${gate_target}" >&2
    exit 0
    ;;
  esac
done

exit 0
