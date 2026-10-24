#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Exercise the trusted upload identity boundary with synthetic GitHub API data.
# The deterministic oracle is the emitted upload identity, a refusal exception,
# or no outputs for stale/closed heads and unsuccessful lanes; no network/waits.
if ! command -v node >/dev/null 2>&1; then
    echo "SKIP: PR coverage source contract requires Node.js"
    exit 77
fi
exec node "${srcdir:-$(dirname "$0")}/pr-coverage-source.cjs"
