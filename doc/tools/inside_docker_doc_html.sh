#!/bin/bash
# Build rsyslog documentation inside the rsyslog/rsyslog_dev_doc_base_ubuntu container.
# Expects GOOGLE_ANALYTICS_ID in env when GA tracking is desired.
# Run with: docker run -e GOOGLE_ANALYTICS_ID -v "$(pwd)":/rsyslog --entrypoint /rsyslog/doc/tools/inside_docker_doc_html.sh rsyslog/rsyslog_dev_doc_base_ubuntu:22.04

set -euo pipefail

cd /rsyslog/doc

# Use pre-built venv from image (prepared in Dockerfile; needed when running as -u uid:gid)
VENV=/opt/rsyslog-doc-venv

# Use env vars for version/release_type (conf.py reads RSYSLOG_DOC_* when .git not in container)
export RSYSLOG_DOC_VERSION="${RSYSLOG_DOC_VERSION:-8}"
export RSYSLOG_DOC_RELEASE_TYPE="${RSYSLOG_DOC_RELEASE_TYPE:-stable}"

rm -rf build/html
nice "$VENV/bin/sphinx-build" -j"$(nproc)" -t with_sitemap -b html -D html_theme=furo \
  -W --keep-going source build/html

"$VENV/bin/python3" tools/fix-mermaid-offline.py build/html
