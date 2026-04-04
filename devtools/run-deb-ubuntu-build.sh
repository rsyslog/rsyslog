#!/bin/bash
# CI helper: build Ubuntu .deb packages from current source.
# Re-exec with CRLF stripped when run from Windows/WSL bind mount (avoids "syntax error near fi")
if grep -q $'\r' "$0" 2>/dev/null; then
  _r="$(cd "$(dirname "$0")/.." && pwd)"
  exec env _RSYSLOG_REPO_ROOT="$_r" bash <(sed 's/\r$//' "$0") "$@"
fi
# Run from repo root. Requires: dpkg-dev, lintian, wget, docker.
#
# Usage: ./run-deb-ubuntu-build.sh [SUITE]
#        bash devtools/run-deb-ubuntu-build.sh [SUITE]  # if ./ fails (e.g. CRLF on Windows)
#   SUITE: focal, jammy, noble, or "all" (default: all)
#
# Env: UBUNTU_VERSION (optional; CI sets BASE-PRNUM-TIMESTAMP; local uses BASE-local-TIMESTAMP)
#
# Steps: install deps; for each suite: clean artifacts, build-ubuntu.sh --suite SUITE.

set -e

REPO_ROOT="${_RSYSLOG_REPO_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)}"
cd "$REPO_ROOT"

source "$REPO_ROOT/packaging/ubuntu/config.sh"

SUITE_ARG="${1:-all}"

echo "-------------------------------------"
echo "--- Ubuntu package build ---"
echo "-------------------------------------"

# Resolve suites
if [ "$SUITE_ARG" = "all" ]; then
  SUITES="$SUITE_OPTIONS"
else
  if [[ ! " $SUITE_OPTIONS " =~ " $SUITE_ARG " ]]; then
    echo "Error: Invalid suite '$SUITE_ARG'. Use: $SUITE_OPTIONS or all" >&2
    exit 1
  fi
  SUITES="$SUITE_ARG"
fi

# Ensure deps
echo "== Install dpkg-dev, lintian, wget =="
if [ "$(id -u)" -eq 0 ]; then
  apt-get update
  apt-get install -y dpkg-dev lintian wget
else
  sudo apt-get update
  sudo apt-get install -y dpkg-dev lintian wget
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "Error: docker required for binary build. Install docker.io or use build-ubuntu.sh --source-only" >&2
  exit 1
fi

FAILED=0
for SUITE in $SUITES; do
  echo ""
  echo "========== Building $SUITE =========="
  if [ -n "$(ls -d rsyslog-[0-9]*/ rsyslog_*.dsc 2>/dev/null)" ]; then
    echo "Cleaning artifacts from previous run..."
    rm -rf -- rsyslog-[0-9]*/ rsyslog_*.dsc rsyslog_*.changes rsyslog_*.buildinfo 2>/dev/null || true
  fi
  if bash "$REPO_ROOT/packaging/ubuntu/build-ubuntu.sh" --suite "$SUITE"; then
    echo "OK: $SUITE"
  else
    echo "FAILED: $SUITE"
    FAILED=1
  fi
done

if [ "${FAILED:-0}" -eq 0 ]; then
  echo ""
  echo "All suites built successfully."
  exit 0
else
  echo ""
  echo "Some builds failed."
  exit 1
fi
