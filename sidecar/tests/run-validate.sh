#!/usr/bin/env bash
## run-validate.sh: Run the sidecar validator with a local venv.
##
## Usage:
##   ./tests/run-validate.sh
##
## This script creates .venv in the sidecar directory if missing,
## installs requirements.txt, and runs the sample and exact-label validators.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIDECAR_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENV_DIR="${SIDECAR_DIR}/.venv"

if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
  python3 -m venv "${VENV_DIR}"
fi

"${VENV_DIR}/bin/pip" install -r "${SIDECAR_DIR}/requirements.txt"

# Parser validation must not start the import-time default UDP listener.
export IMPSTATS_MODE=file IMPSTATS_PATH=/dev/null IMPSTATS_FORMAT=prometheus
"${VENV_DIR}/bin/python" "${SCRIPT_DIR}/validate.py"
exec "${VENV_DIR}/bin/python" "${SIDECAR_DIR}/../tests/sidecar-prometheus-labels.py"
