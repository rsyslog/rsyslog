#!/bin/sh
# Helper for mmexternal interface.output="none" tests.
# It records received lines and intentionally emits no stdout response.
out="$1"

while IFS= read -r line; do
	printf '%s\n' "$line" >> "$out"
done
