#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the optional Python sidecar's file parser and emitted sample labels.
# The pure unit test forces file mode and rejects import-time socket creation;
# missing Python 3.8 or optional sidecar dependencies are an explicit skip.
. ${srcdir:=.}/diag.sh init

SIDECAR_PYTHON="${SIDECAR_TEST_PYTHON:-python3}"
command -v "$SIDECAR_PYTHON" >/dev/null 2>&1 || skip_test "Python 3 is unavailable"
"$SIDECAR_PYTHON" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' \
    || skip_test "sidecar tests require Python 3.8 or newer"
"$SIDECAR_PYTHON" -c 'import prometheus_client; import werkzeug' \
    || skip_test "optional sidecar Python dependencies are unavailable"
"$SIDECAR_PYTHON" "$srcdir/sidecar-prometheus-labels.py" || error_exit 1
exit_test
