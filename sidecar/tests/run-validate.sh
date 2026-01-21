#!/usr/bin/env bash
## run-validate.sh: Run the sidecar validator with a local venv.
##
## Usage:
##   ./tests/run-validate.sh
##
## This script creates .venv in the sidecar directory if missing,
## installs requirements.txt, and runs tests/validate.py.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIDECAR_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENV_DIR="${SIDECAR_DIR}/.venv"

if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
  python3 -m venv "${VENV_DIR}"
fi

"${VENV_DIR}/bin/pip" install -r "${SIDECAR_DIR}/requirements.txt"

exec "${VENV_DIR}/bin/python" "${SCRIPT_DIR}/validate.py"
