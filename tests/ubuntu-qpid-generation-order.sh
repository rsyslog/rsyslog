#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Verify the actual Jammy/Noble helpers complete generated artifacts before
# parallel consumers and preserve configure/build/install sequencing. This is
# a deterministic contract test with build-tool adapters, not a Qpid race
# reproduction. Packaging helpers are not shipped in every source tarball.
set -eu
command -v python3 >/dev/null 2>&1 || exit 77
srcdir=${srcdir:-.}
test -f "$srcdir/../packaging/ubuntu/noble/v8-stable/debian/build-qpid-proton.sh" || exit 77
exec python3 "$srcdir/unit/qpid_generation_order.py"
