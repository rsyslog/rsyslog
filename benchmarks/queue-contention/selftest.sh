#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the exact-ID benchmark oracle accepts a complete range and rejects one
# missing ID and one duplicate ID. It uses the real testbench chkseq binary so
# the benchmark's corruption checks do not rely on a reimplemented oracle.

set -euo pipefail

chkseq=${1:-./chkseq}
if [[ ! -x "$chkseq" ]]; then
    echo "chkseq is not executable: $chkseq" >&2
    exit 2
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
printf '0\n1\n2\n' > "$tmpdir/good"
printf '0\n2\n' > "$tmpdir/missing"
printf '0\n1\n1\n2\n' > "$tmpdir/duplicate"

"$chkseq" -s0 -e2 < "$tmpdir/good"
if "$chkseq" -s0 -e2 < "$tmpdir/missing"; then
    echo "missing-ID fixture unexpectedly passed" >&2
    exit 1
fi
if "$chkseq" -s0 -e2 < "$tmpdir/duplicate"; then
    echo "duplicate-ID fixture unexpectedly passed" >&2
    exit 1
fi
printf 'queue-contention exact-ID oracle selftest passed\n'
