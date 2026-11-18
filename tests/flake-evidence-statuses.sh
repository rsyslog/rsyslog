#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Real Automake FAIL/ERROR/XPASS must retain test failure evidence, whereas
# PASS/SKIP/XFAIL and build-only distcheck failures must not create evidence.
# The Python oracle generates real driver results and checks both collectors;
# no daemon, external services, background helpers, or waits are involved.
# The tooling needs Python 3.9 (dict union and str.removeprefix), so older
# testbench platforms skip this tooling-only check explicitly.
set -eu
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: flake evidence tooling requires Python 3.9 or newer"
    exit 77
fi
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 77)' || {
    echo "SKIP: flake evidence tooling requires Python 3.9 or newer"
    exit 77
}
source_root=$(CDPATH='' cd -- "${srcdir:-$(dirname "$0")}/.." && pwd)
export PYTHONDONTWRITEBYTECODE=1
exec python3 "$source_root/devtools/tests/test_flake_evidence.py" \
    FlakeEvidenceTests.test_real_automake_results_are_classified \
    FlakeEvidenceTests.test_prepare_rejects_distcheck_build_failure
