#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

{
	echo "ARGV_BEGIN"
	while [ $# -gt 0 ]; do
		printf 'ARG:%s\n' "$1"
		shift
	done
	echo "ARGV_END"
	echo "STDIN_BEGIN"
	cat
	echo "STDIN_END"
} >> "$RSYSLOG_OUT_LOG"

exit "${OMMAIL_SENDMAIL_EXIT:-0}"
