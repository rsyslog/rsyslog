#!/usr/bin/env bash
set -euo pipefail

if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
  echo "Error: must run inside a Git working tree." >&2
  exit 2
fi

{
  git diff --diff-filter=d --name-only -- '*.c' '*.h'
  git diff --cached --diff-filter=d --name-only -- '*.c' '*.h'
} | awk 'NF && !seen[$0]++'
