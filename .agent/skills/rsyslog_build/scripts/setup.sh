#!/bin/bash
# .agent/skills/rsyslog_build/scripts/setup.sh
# Standard environment setup for rsyslog agents.

set -e

REPO_ROOT=$(git rev-parse --show-toplevel)

echo ">>> Setting up rsyslog development environment..."

# 1. Run the official setup script if it exists
if [ -f "$REPO_ROOT/devtools/codex-setup.sh" ]; then
    echo ">>> Running devtools/codex-setup.sh..."
    bash "$REPO_ROOT/devtools/codex-setup.sh"
else
    echo ">>> WARNING: devtools/codex-setup.sh not found. Ensure dependencies are installed manually."
    echo ">>> Required: autoconf, automake, libtool, bison, flex, libcurl-dev, libfastjson-dev, etc."
fi

# 2. Check for critical tools
for tool in autoconf automake libtool bison flex; do
    if ! command -v $tool >/dev/null 2>&1; then
        echo ">>> ERROR: Required tool '$tool' is missing."
        exit 1
    fi
done

echo ">>> Setup complete."
