#!/usr/bin/env bash
#
## Start rsyslog Prometheus exporter (file mode)
##
## Uses venv if present, otherwise falls back to system python.
## You can override any env var by exporting it before running this script.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PYTHON="python3"
if [[ -x "$SCRIPT_DIR/.venv/bin/python" ]]; then
  PYTHON="$SCRIPT_DIR/.venv/bin/python"
elif [[ -x "$SCRIPT_DIR/venv/bin/python3" ]]; then
  PYTHON="$SCRIPT_DIR/venv/bin/python3"
fi

export IMPSTATS_MODE="${IMPSTATS_MODE:-file}"
export IMPSTATS_FORMAT="${IMPSTATS_FORMAT:-json}"
export IMPSTATS_PATH="${IMPSTATS_PATH:-$SCRIPT_DIR/examples/sample-impstats.json}"

export LISTEN_ADDR="${LISTEN_ADDR:-127.0.0.1}"
export LISTEN_PORT="${LISTEN_PORT:-9898}"

export LOG_LEVEL="${LOG_LEVEL:-INFO}"

exec "$PYTHON" "$SCRIPT_DIR/rsyslog_exporter.py"
