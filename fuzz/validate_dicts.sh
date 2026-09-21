#!/bin/sh
set -eu

status=0
for dict in "$@"; do
	if [ ! -f "$dict" ]; then
		printf '%s: missing\n' "$dict" >&2
		status=1
		continue
	fi
	awk '
		/^[[:space:]]*($|#)/ { next }
		/^([[:alnum:]_]+[[:space:]]*=[[:space:]]*)?"([^"\\]|\\.)*"([[:space:]]*#.*)?$/ { next }
		{
			printf "%s:%d: invalid AFL++ dictionary line: %s\n", FILENAME, FNR, $0 > "/dev/stderr"
			bad = 1
		}
		END { exit bad ? 1 : 0 }
	' "$dict" || status=1
done
exit "$status"
