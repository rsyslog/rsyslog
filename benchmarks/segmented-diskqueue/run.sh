#!/bin/sh
# Run reproducible segmented disk queue benchmarks through the Python driver.
set -eu

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$script_dir/runner.py" "$@"
