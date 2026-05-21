#!/bin/sh

if [ "${1:-}" = "--invalid" ]; then
    exit 2
fi

echo "OK"

while IFS= read -r line; do
    echo "OK"
done
