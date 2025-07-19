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

# Now we check if the build system is actually initialized - AI Agents often
# "forget" that part, leading to many issues. So we try to auto-fix.

# If configure is absent or not executable, regenerate it
if [ ! -x "$PROJECT_ROOT/configure" ]; then
  echo "[diag] 'configure' missing in $PROJECT_ROOT — running autoreconf"
  # ensure autoreconf is available
  if ! command -v autoreconf >/dev/null 2>&1; then
    echo "Error: 'autoreconf' not found. Please install autoconf/automake." >&2
    exit 1
  fi
  # run in a subshell so we don’t disturb the caller’s cwd
  (
    cd "$PROJECT_ROOT" || exit 1
    autoreconf -fi
  ) || {
    echo "[diag] Error: autoreconf failed in '$PROJECT_ROOT'." >&2
    exit 1
  }
fi

if [ ! -f "$PROJECT_ROOT/Makefile" ]; then
  echo "[diag] Makefile missing in $PROJECT_ROOT — running configure"
  # sanity check: configure must exist & be executable
  if [ ! -x "$PROJECT_ROOT/configure" ]; then
    echo "Error: '$PROJECT_ROOT/configure' not found or not executable." >&2
    exit 1
  fi
  (
    cd "$PROJECT_ROOT" || { echo "Cannot cd to $PROJECT_ROOT" >&2; exit 1; }
    ./configure --enable-testbench --enable-impstats --enable-imdiag --enable-imtcp \
      --enable-imfile --enable-omstdout \
      || { echo "configure failed" >&2; exit 1; }
  )
fi

# paths to the helper binaries
TCPFLOOD_BIN="$PROJECT_ROOT/tests/tcpflood"
RSYSLOGD_BIN="$PROJECT_ROOT/tools/rsyslogd"

# if tcpflood or rsyslogd is missing/not executable, build test tools
if [ ! -x "$TCPFLOOD_BIN" ] || [ ! -x "$RSYSLOGD_BIN" ]; then
  missing_tools_msg=""
  [ ! -x "$TCPFLOOD_BIN" ] && missing_tools_msg="tcpflood"
  if [ ! -x "$RSYSLOGD_BIN" ]; then
    [ -n "$missing_tools_msg" ] && missing_tools_msg="$missing_tools_msg, "
    missing_tools_msg="${missing_tools_msg}rsyslogd"
  fi
  echo "[diag] helper tools missing ($missing_tools_msg)—building test tools"

  # determine number of parallel jobs
  if command -v nproc >/dev/null 2>&1; then
    NPROC="$(nproc)"
  else
    NPROC="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
  fi

  # run make in a subshell so we don't change the caller's cwd
  (
    cd "$PROJECT_ROOT" || { echo "Error: cannot cd to $PROJECT_ROOT" >&2; exit 1; }
    make -j"$NPROC" check TESTS="" \
      || { echo "Error: make check failed" >&2; exit 1; }
  )
fi

## --- back to regular processing

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

