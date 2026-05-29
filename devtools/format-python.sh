#!/bin/sh
# Check Python style, optionally formatting first with autopep8.

set -eu

usage() {
  cat <<'EOF'
Usage: devtools/format-python.sh [--fix] [--check-if-available] [file ...]

Checks Python files with pycodestyle using setup.cfg. With --fix, formats the
files first with autopep8. When no files are supplied, all tracked Python files
are processed.

--check-if-available exits successfully when pycodestyle is not installed after
printing an install suggestion. This is for optional local agent checks only; CI
installs pycodestyle explicitly.
EOF
}

fix=false
optional=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --help|-h)
      usage
      exit 0
      ;;
    --fix)
      fix=true
      shift
      ;;
    --check-if-available)
      optional=true
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      break
      ;;
  esac
done

if ! command -v pycodestyle >/dev/null 2>&1; then
  echo "pycodestyle is not installed. Suggested install: sudo apt-get install -y pycodestyle" >&2
  if [ "$optional" = true ]; then
    exit 0
  fi
  exit 1
fi

if [ "$fix" = true ] && ! command -v autopep8 >/dev/null 2>&1; then
  echo "autopep8 is not installed. Suggested install: sudo apt-get install -y python3-autopep8" >&2
  if [ "$optional" = true ]; then
    exit 0
  fi
  exit 1
fi

file_list=
if [ "$#" -eq 0 ]; then
  file_list=$(mktemp)
  git ls-files -z '*.py' > "$file_list"
  trap 'rm -f "$file_list"' EXIT HUP INT TERM
  if [ ! -s "$file_list" ]; then
    echo "No tracked Python files found."
    exit 0
  fi
  if [ "$fix" = true ]; then
    xargs -0 autopep8 --in-place < "$file_list"
  fi
  xargs -0 pycodestyle < "$file_list"
else
  if [ "$fix" = true ]; then
    autopep8 --in-place "$@"
  fi
  pycodestyle "$@"
fi
