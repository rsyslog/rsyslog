#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Initialize the testbench first so the Solaris TLS subset guard applies.
# The Python harness owns the listener and daemon, and verifies their lifecycle.
set -e
. ${srcdir:=.}/diag.sh init
python3 "${srcdir}/omdtls-server-identity.py"
exit_test
