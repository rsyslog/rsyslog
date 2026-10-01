#!/bin/sh
# Run the Mermaid import-path regression when Python 3 is available.
# Exit 77 without Python so Automake reports a skip rather than a failure.
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: mermaid-offline requires Python 3"
    exit 77
fi

exec python3 "${srcdir:-$(dirname "$0")}/mermaid-offline.py"
