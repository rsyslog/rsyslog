#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# TEST ONLY / DO NOT MERGE. Debian package sources, not this PR source.
set -euo pipefail
arch="${1:?usage: test-debian-repro.sh amd64|arm64|armhf}"
case "$arch" in
  amd64|arm64|armhf) ;;
  *) echo "unsupported architecture: $arch" >&2; exit 2 ;;
esac
root="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$root/devtools/debian-repro-experiment.py" \
  --arch "$arch" --output "$root/.ci/debian-repro-$arch"
