#!/bin/bash

outfile=$1

echo "Starting" >> "$outfile"
trap 'echo "Terminating" >> "$outfile"; exit 0' TERM INT

while IFS= read -r line; do
	echo "Received $line" >> "$outfile"
	printf '{}\n'
done

echo "Terminating" >> "$outfile"
