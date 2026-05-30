#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
  echo "Error: must run inside a Git working tree." >&2
  exit 2
fi

base_ref="${RSYSLOG_LOCAL_VALIDATION_BASE:-origin/main}"

{
  if git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
    git diff --diff-filter=d --name-only "$base_ref"...HEAD -- '*.c' '*.h'
  fi
  git diff --diff-filter=d --name-only HEAD -- '*.c' '*.h'
  git ls-files --others --exclude-standard -- '*.c' '*.h'
} | awk 'NF && !seen[$0]++'
