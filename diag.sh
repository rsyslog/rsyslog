#!/usr/bin/env bash
## diag.sh stub for rsyslog test harness forwarding
##
## This stub lives at the repo root so that every test’s
## “. ${srcdir:=.}/diag.sh” line loads the real harness in tests/.
##
## It performs:
##   1) Locate project root (where this stub lives)
##   2) Ensure srcdir points to tests/ under project root
##   3) Identify which test script sourced this stub
##   4) Change into that test’s directory so relative paths work
##      – If `cd` fails (e.g. malformed caller path or missing dir),
##        the stub aborts immediately with a clear error. This prevents
##        sourcing the wrong harness or causing misleading test failures.
##   5) Print a one-line notice about forwarding
##   6) Source the real tests/diag.sh, passing any args (init, kill, etc.)
##
## Note on directory management:
##   We could use `pushd`/`popd` to manage a directory stack, but since
##   each test script runs in its own shell process, a simple `cd` is
##   sufficient. No `popd` is needed—when the test process exits, its
##   working directory is discarded.
##
## Usage in test scripts:
##   . ${srcdir:=.}/diag.sh init
##   → this stub handles locating and sourcing tests/diag.sh automatically.

# 1) Determine project root (where this stub lives)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 2) Point srcdir at the real harness directory
: "${srcdir:="${PROJECT_ROOT}/tests"}"

# 3) Find which test script called us (strip leading "./")
CALLER="${BASH_SOURCE[1]#./}"         # e.g. "tests/foo.sh"
TEST_DIR="${PROJECT_ROOT}/$(dirname "${CALLER}")"

# 4) Notify user/agent of forwarding action
echo "diag.sh stub: forwarding to ${srcdir}/diag.sh in ${TEST_DIR}"

# 5) Change into the test’s directory (abort on failure)
cd "${TEST_DIR}" || {
  echo "FATAL: cannot cd to ${TEST_DIR}" >&2
  exit 1
}

# 6) Source the real harness, passing along all arguments
. "${srcdir}/diag.sh" "$@"

