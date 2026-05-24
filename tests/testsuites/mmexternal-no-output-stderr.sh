#!/bin/sh
# Helper for mmexternal interface.output="none" tests.
# It writes more stderr data than a pipe can buffer before recording the input
# line, so ignored child output must not be connected to an unread pipe.
out="$1"

while IFS= read -r line; do
	dd if=/dev/zero bs=4096 count=512 >&2 2>/dev/null
	printf '%s\n' "$line" >> "$out"
done
