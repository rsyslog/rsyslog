#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
pidfile="$1"
block_fifo="${pidfile}.fifo"

rm -f "$block_fifo"
mkfifo "$block_fifo" || exit 1

echo "$$" > "$pidfile"
echo "program data"

read -r _ < "$block_fifo"
