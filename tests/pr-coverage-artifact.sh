#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Exercise the hostile ZIP/LCOV data boundary without a daemon or network.
# Rejection must raise an error before writing output, leaving an outside
# sentinel intact; positive cases must produce the expected normalized report.
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: PR coverage artifact contract requires Python 3"
    exit 77
fi
exec python3 "${srcdir:-$(dirname "$0")}/pr-coverage-artifact.py"
