#!/usr/bin/env bash
set -euo pipefail

# copilot-policy-alloc.sh
#
# Purpose:
#   Multi-stage rsyslog policy check pipeline using GitHub Copilot CLI.
#   Runs checks from least to most costly, stopping at first issue.
#
# Default diff:
#   main...HEAD
#
# Overrides:
#   ./copilot-policy-alloc.sh origin/main...HEAD
#   ./copilot-policy-alloc.sh --cached
#
# Output:
#   JSON only (machine-consumable)
#
# Exit codes:
#   0  all checks passed (or no changes)
#   1  policy violations found
#   3  tool/setup error

# Configuration
POLICY_CHECK_MODEL="gpt-5.1-codex-mini"
CUBIC_BASE_BRANCH="main"
DEBUG=false
SKIP_STAGES=()
FORCE_SUBCLI=""

die() {
  echo "error: $*" >&2
  exit 3
}

show_help() {
  cat <<'HELP'
Usage: copilot-policy-alloc.sh [OPTIONS] [DIFF_SPEC]

Multi-stage rsyslog policy check pipeline using AI code review tools.
Runs checks from least to most costly, stopping at first issue.

OPTIONS:
  --debug                     Enable debug output to stderr
  --skip <stage>              Skip a pipeline stage (repeatable)
                              Values: alloc-policy, mock-distcheck, cubic
  --subcli <tool>             Force specific CLI tool
                              Values: copilot, codex
  --cached                    Use staged changes instead of commits
  -h, --help                  Show this help message

DIFF_SPEC:
  Default: main...HEAD
  Examples: origin/main...HEAD, HEAD~1..HEAD

PIPELINE STAGES:
  1. alloc-policy   - Scans .c/.h files for raw malloc/calloc/realloc/strdup calls
  2. mock-distcheck - Runs 'make distcheck TEST_RUN_TYPE=MOCK-OK' for packaging validation
  3. cubic          - Runs cubic code review (final stage)

NOTE: Agent should build/test working tree BEFORE running this script.

EXIT CODES:
  0  All checks passed (or no changes/skipped)
  1  Policy violations found
  3  Tool/setup error

EXAMPLES:
  copilot-policy-alloc.sh --debug
  copilot-policy-alloc.sh --skip cubic
  copilot-policy-alloc.sh --skip alloc-policy --skip cubic
  copilot-policy-alloc.sh --skip mock-distcheck
  copilot-policy-alloc.sh --subcli copilot --debug
  copilot-policy-alloc.sh --subcli codex origin/main...HEAD
HELP
  exit 0
}

debug() {
  if $DEBUG; then
    echo "[DEBUG] $*" >&2
  fi
}

should_skip() {
  local stage="$1"
  for skip in "${SKIP_STAGES[@]}"; do
    if [[ "$skip" == "$stage" ]]; then
      return 0
    fi
  done
  return 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

need_cmd git
# Either codex or copilot required (check later)

has_issues() {
  local output="$1"
  # Try JSON first (cubic, codex output)
  if has_issues_array "$output"; then
    return 0
  fi
  # For text streams, check if output is non-empty (skip common "success" patterns)
  if [[ -n "$output" ]] && ! echo "$output" | grep -qE '^(\s*|\{\s*"issues"\s*:\s*\[\s*\]\s*\})$'; then
    return 0
  fi
  return 1
}

has_issues_array() {
  local json="$1"
  # Check if issues array has elements: "issues":[...] with content
  echo "$json" | grep -q '"issues"\s*:\s*\[[^]]\+\]'
}

check_allocation_policy() {
  local diff_file="$1"

  # Read diff content
  local diff_content
  diff_content=$(<"$diff_file")

  # Compact, CLI-safe policy prompt
  local PROMPT=$'IMPORTANT: Do NOT read AGENTS.md, DEVELOPING.md, or other repo guidance files. This is a simple pattern-matching task.\n\n'
  PROMPT+=$'Workflow: rsyslog policy-alloc\n\n'
  PROMPT+=$'Task: Scan ONLY added/modified lines in the diff for raw allocation calls:\n'
  PROMPT+=$'malloc(, calloc(, realloc(, strdup(, strndup(\n'
  PROMPT+=$'Regex: \\b(malloc|calloc|realloc|strdup|strndup)\\s*\\(\n\n'
  PROMPT+=$'A match is a violation unless the same statement/expression clearly uses an approved project wrapper/macro '
  PROMPT+=$'(e.g. CHKmalloc/CHKrealloc or project strdup wrapper). '
  PROMPT+=$'If uncertain, report as warning with lower confidence.\n\n'
  PROMPT+=$'Output JSON ONLY (no prose). Match this exact format:\n'
  PROMPT+=$'{\n'
  PROMPT+=$'  "issues": [\n'
  PROMPT+=$'    {\n'
  PROMPT+=$'      "file": "<path>",\n'
  PROMPT+=$'      "line": <int>,\n'
  PROMPT+=$'      "severity": "error|warning|info",\n'
  PROMPT+=$'      "code": "ALLOC_RAW_CALL",\n'
  PROMPT+=$'      "message": "<description>",\n'
  PROMPT+=$'      "suggestion": "<recommended fix>"\n'
  PROMPT+=$'    }\n'
  PROMPT+=$'  ]\n'
  PROMPT+=$'}\n\n'
  PROMPT+=$'If no issues found, return: {"issues":[]}\n\n'
  PROMPT+=$'Analyze this diff:\n\n'
  PROMPT+="$diff_content"

  if $DEBUG; then
    debug "=== Prompt content (first 500 chars) ==="
    echo "${PROMPT:0:500}..." >&2
    debug "=== Diff file size: $(wc -l < "$diff_file") lines ==="
    debug "=== Diff file content preview (first 20 lines) ==="
    head -20 "$diff_file" >&2
  fi

  # Prefer codex (faster), fall back to copilot, or force via --subcli
  if [[ "$FORCE_SUBCLI" == "codex" ]] || { [[ -z "$FORCE_SUBCLI" ]] && command -v codex >/dev/null 2>&1; }; then
    if [[ "$FORCE_SUBCLI" == "codex" ]] && ! command -v codex >/dev/null 2>&1; then
      die "Forced --subcli codex but codex not found"
    fi
    debug "Using codex for policy check"
    debug "Command: codex exec --model $POLICY_CHECK_MODEL --dangerously-bypass-approvals-and-sandbox <PROMPT>"
    if $DEBUG; then
      codex exec \
        --model "$POLICY_CHECK_MODEL" \
        --dangerously-bypass-approvals-and-sandbox \
        "$PROMPT"
    else
      # Suppress codex's verbose output (stderr) when not in debug mode
      codex exec \
        --model "$POLICY_CHECK_MODEL" \
        --dangerously-bypass-approvals-and-sandbox \
        "$PROMPT" 2>/dev/null
    fi
  elif [[ "$FORCE_SUBCLI" == "copilot" ]] || { [[ -z "$FORCE_SUBCLI" ]] && command -v copilot >/dev/null 2>&1; }; then
    if [[ "$FORCE_SUBCLI" == "copilot" ]] && ! command -v copilot >/dev/null 2>&1; then
      die "Forced --subcli copilot but copilot not found"
    fi
    debug "Using copilot for policy check"
    debug "Command: copilot --model $POLICY_CHECK_MODEL --add-dir /tmp --allow-all-tools --prompt <PROMPT>"
    copilot \
      --model "$POLICY_CHECK_MODEL" \
      --add-dir /tmp \
      --allow-all-tools \
      --prompt "$PROMPT"
  else
    debug "Neither codex nor copilot CLI found, skipping check"
    # Return valid empty JSON matching cubic format
    echo '{"issues":[]}'
  fi
}

run_cubic_review() {
  if ! command -v cubic >/dev/null 2>&1; then
    return 0
  fi

  debug "Command: cubic review --base $CUBIC_BASE_BRANCH --json"
  cubic review --base "$CUBIC_BASE_BRANCH" --json 2>&1 || true
}

run_mock_distcheck() {
  debug "Mock distcheck: Running make distcheck TEST_RUN_TYPE=MOCK-OK..."
  
  # Run distcheck - let output stream directly (don't capture)
  # Return exit code for caller to check
  if ! make distcheck TEST_RUN_TYPE=MOCK-OK -j$(nproc) 2>&1; then
    return 1  # Signal failure
  fi
  return 0  # Success
}

# Parse arguments
DIFF_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      show_help
      ;;
    --debug)
      DEBUG=true
      shift
      ;;
    --skip)
      if [[ $# -lt 2 ]]; then
        die "--skip requires an argument (alloc-policy|mock-distcheck|cubic)"
      fi
      SKIP_STAGES+=("$2")
      shift 2
      ;;
    --subcli)
      if [[ $# -lt 2 ]]; then
        die "--subcli requires an argument (copilot|codex)"
      fi
      FORCE_SUBCLI="$2"
      shift 2
      ;;
    --cached)
      DIFF_ARGS+=(--cached)
      shift
      ;;
    *)
      DIFF_ARGS+=("$1")
      shift
      ;;
  esac
done

# Default diff target if none specified
if [[ ${#DIFF_ARGS[@]} -eq 0 ]]; then
  DIFF_ARGS+=("main...HEAD")
fi

debug "Debug mode enabled"
debug "Diff args: ${DIFF_ARGS[*]}"
if [[ ${#SKIP_STAGES[@]} -gt 0 ]]; then
  debug "Skipping stages: ${SKIP_STAGES[*]}"
fi
if [[ -n "$FORCE_SUBCLI" ]]; then
  debug "Forcing subcli: $FORCE_SUBCLI"
fi

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || die "not a git repo?"
cd "$REPO_ROOT"
debug "Repository root: $REPO_ROOT"

TMP_DIFF="$(mktemp -p /tmp copilot_policy_alloc.XXXXXX.diff)"
cleanup() { 
  rm -f "$TMP_DIFF"
  # distcheck leaves read-only directories - restore write perms before removing
  if compgen -G "rsyslog-*.daily" > /dev/null 2>&1; then
    chmod -R u+w rsyslog-*.daily 2>/dev/null || true
    rm -rf rsyslog-*.daily
  fi
  rm -f rsyslog-*.tar.gz 2>/dev/null || true
}
trap cleanup EXIT
debug "Temp diff file: $TMP_DIFF"

# Generate minimal diff (unified=0 keeps token usage low)
debug "Generating diff..."
debug "Command: git diff --unified=0 --no-color ${DIFF_ARGS[*]}"
git diff --unified=0 --no-color "${DIFF_ARGS[@]}" >"$TMP_DIFF" || true

# Also include unstaged changes in working directory
if [[ "${DIFF_ARGS[*]}" != *"--cached"* ]]; then
  debug "Command: git diff --unified=0 --no-color (unstaged)"
  git diff --unified=0 --no-color >>"$TMP_DIFF" || true
fi

# Check for untracked files
debug "Command: git ls-files --others --exclude-standard"
UNTRACKED_FILES=$(git ls-files --others --exclude-standard)
if [[ -n "$UNTRACKED_FILES" ]]; then
  debug "Found untracked files:"
  $DEBUG && echo "$UNTRACKED_FILES" >&2

  # Add untracked file contents to diff as proper unified diffs
  # Skip: the script itself, build artifacts, and generated tarballs
  while IFS= read -r file; do
    if [[ -f "$file" && "$file" != "ai/copilot-policy-alloc.sh" && "$file" != rsyslog-*.daily/* && "$file" != *.tar.gz ]]; then
      line_count=$(wc -l < "$file")
      # Limit file size to prevent argument list too long
      if [[ $line_count -lt 10000 ]]; then
        echo "diff --git a/$file b/$file" >>"$TMP_DIFF"
        echo "new file mode 100644" >>"$TMP_DIFF"
        echo "index 0000000..0000000" >>"$TMP_DIFF"
        echo "--- /dev/null" >>"$TMP_DIFF"
        echo "+++ b/$file" >>"$TMP_DIFF"
        echo "@@ -0,0 +1,$line_count @@" >>"$TMP_DIFF"
        # Add all lines as additions with proper + prefix (no space)
        sed 's/^/+/' "$file" >>"$TMP_DIFF"
      else
        debug "Skipping large untracked file: $file ($line_count lines)"
      fi
    fi
  done <<< "$UNTRACKED_FILES"
fi

DIFF_IS_EMPTY=false
if [[ ! -s "$TMP_DIFF" ]]; then
  debug "No changes detected: no diff and no untracked files"
  echo '{"issues":[]}'
  exit 0
fi

# Pipeline: Run checks from least to most costly
debug "Starting pipeline checks..."

# Stage 1: Allocation policy check (only for .c/.h files)
if should_skip "alloc-policy"; then
  debug "Stage 1: Skipped via --skip alloc-policy"
  RESULT='{"issues":[]}'
elif grep -q '^[+].*\.\(c\|h\)' "$TMP_DIFF" || git diff --name-only "${DIFF_ARGS[@]}" 2>/dev/null | grep -q '\.\(c\|h\)$'; then
  debug "Stage 1: Running allocation policy check (C/H files detected)..."
  RESULT=$(check_allocation_policy "$TMP_DIFF")
  debug "Stage 1 result:"
  $DEBUG && echo "$RESULT" | head -20 >&2
  if has_issues "$RESULT"; then
    debug "Issues found in Stage 1, stopping pipeline"
    echo "$RESULT"
    exit 1
  fi
  debug "Stage 1 passed"
else
  debug "Stage 1: Skipped (no .c or .h files modified)"
  RESULT='{"issues":[]}'
fi

# Stage 2: Add more checks here (in cost order)
# ...

# Stage 2: Mock distcheck
if should_skip "mock-distcheck"; then
  debug "Stage 2: Skipped via --skip mock-distcheck"
else
  debug "Stage 2: Running mock distcheck..."
  if ! run_mock_distcheck; then
    debug "Issues found in Stage 2, stopping pipeline"
    exit 1
  fi
  debug "Stage 2 passed"
fi

# Final stage: Run cubic if available
debug "Final stage: Checking for cubic..."
if should_skip "cubic"; then
  debug "Cubic: Skipped via --skip cubic"
else
  CUBIC_OUTPUT=$(run_cubic_review)
  if [[ -n "$CUBIC_OUTPUT" ]]; then
    debug "Cubic output received:"
    $DEBUG && echo "$CUBIC_OUTPUT" | head -20 >&2
    # Check if cubic JSON has non-empty issues array
    if has_issues_array "$CUBIC_OUTPUT"; then
      debug "Cubic detected issues"
      echo "$CUBIC_OUTPUT"
      exit 1
    fi
    debug "Cubic ran but found no issues"
  else
    debug "Cubic not installed or no output"
  fi
fi

# All checks passed
debug "All pipeline checks passed"
echo "$RESULT"
exit 0

