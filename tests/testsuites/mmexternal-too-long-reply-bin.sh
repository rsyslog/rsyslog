#!/bin/bash

outfile=$1
statefile=$2

echo "Starting $$" >> "$outfile"

if [ ! -e "$statefile" ]; then
	: > "$statefile"
	if IFS= read -r line; then
		echo "Oversized $line" >> "$outfile"
		printf '{"msg":"'
		head -c 1200000 /dev/zero | tr '\0' 'A'
		printf '"}'
		sleep 30
	fi
else
	while IFS= read -r line; do
		echo "Recovered $line" >> "$outfile"
		printf '{}\n'
	done
fi

echo "Terminating $$" >> "$outfile"
