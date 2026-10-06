#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

if [ "${1:-}" = "--invalid" ]; then
    exit 2
fi

echo "OK"

while IFS= read -r line; do
    echo "OK"
done
