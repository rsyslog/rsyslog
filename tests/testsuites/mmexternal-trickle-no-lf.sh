#!/bin/sh
# Helper for mmexternal responseTimeout tests.
# It emits partial response bytes without LF long enough to prove the timeout
# applies to the complete response frame rather than each individual read.
marker="$1"

while IFS= read -r _line; do
	i=0
	while [ "$i" -lt 3 ]; do
		printf ' '
		sleep 1
		i=$((i + 1))
	done
	printf '%s\n' completed >> "$marker"
	sleep 60
done
