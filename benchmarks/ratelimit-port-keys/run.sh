#!/bin/sh
# Run reproducible port-key ratelimit benchmarks.
exec "$(dirname "$0")/runner.py" "$@"
