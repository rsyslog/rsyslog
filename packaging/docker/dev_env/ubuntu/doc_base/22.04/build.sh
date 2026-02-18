#!/bin/bash
set -e
# Build from repo root so COPY doc/requirements.txt works for pre-built venv
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../../.." && pwd)"
# Use --no-cache to rebuild image
docker build $1 -f "$SCRIPT_DIR/Dockerfile" -t rsyslog/rsyslog_dev_doc_base_ubuntu:22.04 "$REPO_ROOT"
printf "\n\n================== BUILD DONE\n"
