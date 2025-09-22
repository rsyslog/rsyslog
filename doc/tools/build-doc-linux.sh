#!/usr/bin/env bash
set -euo pipefail

# Simple Linux helper to build rsyslog docs.
# - Creates/uses a Python 3 virtualenv in .venv-docs under doc/
# - Installs requirements from doc/requirements.txt
# - Runs sphinx-build to produce HTML into doc/build

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENV_DIR="${DOC_DIR}/.venv-docs"
REQ_FILE="${DOC_DIR}/requirements.txt"
OUTPUT_DIR="${DOC_DIR}/build"

usage() {
  echo "Usage: $(basename "$0") [--clean] [--strict] [--format html|epub] [--extra \"<opts>\"]" >&2
  echo "  --clean      Remove previous build directory before building" >&2
  echo "  --strict     Treat Sphinx warnings as errors (-W)" >&2
  echo "  --format     Build format (default: html)" >&2
  echo "  --extra      Additional options passed to sphinx-build" >&2
}

CLEAN=0
STRICT=0
FORMAT="html"
EXTRA_OPTS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) CLEAN=1; shift ;;
    --strict) STRICT=1; shift ;;
    --format) FORMAT="${2:-html}"; shift 2 ;;
    --extra) EXTRA_OPTS="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
  esac
done

command -v python3 >/dev/null 2>&1 || { echo "python3 is required" >&2; exit 1; }

# On Debian/Ubuntu/Red Hat, python3-venv may be separate; fallback to virtualenv if venv unavailable.
create_venv() {
  # Remove incomplete venvs (when ensurepip failure created only python links)
  if [[ -d "${VENV_DIR}" && ! -f "${VENV_DIR}/bin/activate" ]]; then
    rm -rf "${VENV_DIR}"
  fi

  if python3 -m venv --help >/dev/null 2>&1; then
    if ! python3 -m venv "${VENV_DIR}" 2>/dev/null; then
      # Fallback to virtualenv if venv creation fails (e.g., ensurepip missing)
      if ! python3 -m pip -q show virtualenv >/dev/null 2>&1; then
        python3 -m pip install --user virtualenv 2>/dev/null || python3 -m pip install --user --break-system-packages virtualenv
      fi
      python3 -m virtualenv "${VENV_DIR}"
    fi
  else
    if ! python3 -m pip -q show virtualenv >/dev/null 2>&1; then
      python3 -m pip install --user virtualenv 2>/dev/null || python3 -m pip install --user --break-system-packages virtualenv
    fi
    python3 -m virtualenv "${VENV_DIR}"
  fi
}

if [[ ! -f "${VENV_DIR}/bin/activate" ]]; then
  echo "Creating virtual environment at ${VENV_DIR}"
  create_venv
fi

source "${VENV_DIR}/bin/activate"

# Ensure pip exists inside the venv; Debian/Ubuntu without ensurepip may lack it
if ! python -m pip --version >/dev/null 2>&1; then
  echo "Bootstrapping pip inside the virtual environment..."
  TMP_DIR="$(mktemp -d)"
  GETPIP="${TMP_DIR}/get-pip.py"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL https://bootstrap.pypa.io/get-pip.py -o "${GETPIP}"
  elif command -v wget >/dev/null 2>&1; then
    wget -qO "${GETPIP}" https://bootstrap.pypa.io/get-pip.py
  else
    echo "Neither curl nor wget available to fetch get-pip.py" >&2
    exit 1
  fi
  python "${GETPIP}"
  rm -rf "${TMP_DIR}"
fi

python -m pip install --upgrade pip
python -m pip install -r "${REQ_FILE}"

[[ ${CLEAN} -eq 1 ]] && rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

SPHINXOPTS="${EXTRA_OPTS}"
if [[ ${STRICT} -eq 1 ]]; then
  SPHINXOPTS="-W ${SPHINXOPTS}"
fi

echo "Building docs: format=${FORMAT} output=${OUTPUT_DIR}"
sphinx-build ${SPHINXOPTS} -b "${FORMAT}" "${DOC_DIR}/source" "${OUTPUT_DIR}"

echo "Done. Open ${OUTPUT_DIR}/index.html for HTML builds."

