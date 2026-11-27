#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Unit tests for the release SBOM generator (devtools/sbom/generate-sbom.py):
# build-system scanning, manifest drift detection, CycloneDX output and the
# --check mode on the real source tree.  No daemon, no network.
# The generator needs Python 3.11+ (tomllib); older Pythons skip with 77.
if ! python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' >/dev/null 2>&1; then
    echo "SKIP: sbom-generator requires Python 3.11+"
    exit 77
fi
exec python3 "${srcdir:-$(dirname "$0")}/sbom-generator.py"
