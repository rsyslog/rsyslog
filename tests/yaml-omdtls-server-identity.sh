#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the same exact certificate identity oracles as the RainerScript
# companion through YAML action parameter parsing. The Python harness owns
# both processes and waits for a mode-specific success or rejection result.
set -e
. ${srcdir:=.}/diag.sh init
python3 "${srcdir}/omdtls-server-identity.py" --yaml
exit_test
