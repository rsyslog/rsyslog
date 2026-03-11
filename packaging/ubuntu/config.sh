# Ubuntu package build configuration for rsyslog packaging/ubuntu/
# Sourced by build-ubuntu.sh and devtools/run-deb-ubuntu-build.sh.

# Supported suites (Ubuntu 20, 22, 24 only)
SUITE_OPTIONS="focal jammy noble"

# Docker image per suite (for binary build)
# focal=20.04, jammy=22.04, noble=24.04
get_suite_image() {
  case "$1" in
    focal) echo "ubuntu:20.04" ;;
    jammy) echo "ubuntu:22.04" ;;
    noble) echo "ubuntu:24.04" ;;
    *) echo "Error: Unknown suite '$1' in get_suite_image" >&2; exit 1 ;;
  esac
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DEBIAN_BASE="$SCRIPT_DIR"
