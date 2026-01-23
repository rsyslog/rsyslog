#!/usr/bin/env bash
#
## Start rsyslog Prometheus exporter (UDP mode)
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

export IMPSTATS_MODE="${IMPSTATS_MODE:-udp}"
export IMPSTATS_FORMAT="${IMPSTATS_FORMAT:-json}"

export IMPSTATS_UDP_ADDR="${IMPSTATS_UDP_ADDR:-127.0.0.1}"
export IMPSTATS_UDP_PORT="${IMPSTATS_UDP_PORT:-19090}"
export STATS_COMPLETE_TIMEOUT="${STATS_COMPLETE_TIMEOUT:-5}"

export LISTEN_ADDR="${LISTEN_ADDR:-127.0.0.1}"
export LISTEN_PORT="${LISTEN_PORT:-9898}"

# Security/DoS knobs (optional)
export MAX_UDP_MESSAGE_SIZE="${MAX_UDP_MESSAGE_SIZE:-65535}"
export MAX_BURST_BUFFER_LINES="${MAX_BURST_BUFFER_LINES:-10000}"
export ALLOWED_UDP_SOURCES="${ALLOWED_UDP_SOURCES:-}"

export LOG_LEVEL="${LOG_LEVEL:-INFO}"

exec "$PYTHON" -m gunicorn \
  --bind "${LISTEN_ADDR}:${LISTEN_PORT}" \
  --workers 1 \
  --threads 2 \
  --timeout 30 \
  --access-logfile - \
  --error-logfile - \
  --log-level "${LOG_LEVEL,,}" \
  rsyslog_exporter:application
