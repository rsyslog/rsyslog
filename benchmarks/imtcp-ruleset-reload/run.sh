#!/bin/sh
# Run paired, alternating imtcp/ruleset reload benchmark trials.
set -eu
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$script_dir/runner.py" "$@"
