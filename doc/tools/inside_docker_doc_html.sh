#!/bin/bash
# Build rsyslog documentation inside the rsyslog/rsyslog_dev_doc_base_ubuntu container.
# Expects GOOGLE_ANALYTICS_ID in env when GA tracking is desired.
# Run with: docker run -e GOOGLE_ANALYTICS_ID -v "$(pwd)":/rsyslog --entrypoint /rsyslog/doc/tools/inside_docker_doc_html.sh rsyslog/rsyslog_dev_doc_base_ubuntu:22.04

set -e

cd /rsyslog/doc

export HOME=/tmp
pip install -q --user -r requirements.txt

# Patch conf.py: upstream has version commented out but uses it in "if version == '8'"
# Also set release_type default (upstream bug: not set when .git not at expected path)
sed -i '/^#version = /a version = '\''8'\''  # Patched for docker build\
release_type = '\''stable'\''  # Patched: default when .git not found' source/conf.py

rm -rf build/html
nice sphinx-build -j8 -t with_sitemap -b html -D html_theme=furo \
  -W -q --keep-going source build/html

python3 tools/fix-mermaid-offline.py build/html
