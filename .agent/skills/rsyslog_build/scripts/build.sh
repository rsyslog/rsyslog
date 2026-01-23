#!/bin/bash
# .agent/skills/rsyslog_build/scripts/build.sh
# Efficient incremental build for rsyslog.

set -e

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

# 1. Auto-bootstrap if needed
if [ ! -f "Makefile" ]; then
    echo ">>> Makefile missing. Bootstrapping..."
    ./autogen.sh --enable-debug --enable-testbench --enable-imdiag --enable-omstdout --enable-mmsnareparse --enable-omotel --enable-imhttp
fi

# 2. Run incremental build
echo ">>> Running incremental build (make -j$(nproc) check TESTS=\"\")..."
make -j$(nproc) check TESTS=""

echo ">>> Build successful."
