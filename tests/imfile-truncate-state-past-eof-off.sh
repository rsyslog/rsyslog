#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Same setup as imfile-truncate-state-past-eof.sh with the flag left off.
# A saved offset past EOF must stay put. After the restart the file is padded
# back to that offset and two new lines are appended. Only those two lines
# may be emitted. The prefix must not be replayed.
. ${srcdir:=.}/diag.sh init
check_command_available truncate

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="polling" PollingInterval="1")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="file:")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

./inputfilegen -m 80 > "$RSYSLOG_DYNNAME.input"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 80 50
shutdown_when_empty
wait_shutdown
seq_check 0 79

prefix_bytes=$(awk '
	{ total += length($0) + 1
	  if (total >= 512) { print total; exit }
	}
' "$RSYSLOG_DYNNAME.input")
if [ -z "$prefix_bytes" ] || [ "$prefix_bytes" -lt 512 ]; then
	printf 'FAIL: input shorter than the file-id window (%s)\n' "$prefix_bytes"
	error_exit 1
fi
truncate -s "$prefix_bytes" "$RSYSLOG_DYNNAME.input"

set -- "$RSYSLOG_DYNNAME".spool/imfile-state:*
if [ "$#" -ne 1 ] || [ ! -f "$1" ]; then
	printf 'FAIL: expected one state file\n'
	ls -l "$RSYSLOG_DYNNAME".spool
	error_exit 1
fi
saved_off=$(sed -n 's/.*"curr_offs": *\([0-9][0-9]*\).*/\1/p' "$1" | head -1)
now_size=$(wc -c < "$RSYSLOG_DYNNAME.input" | tr -d ' ')
if [ -z "$saved_off" ] || [ "$saved_off" -le "$now_size" ]; then
	printf 'FAIL: saved offset [%s] is not past size [%s]\n' "$saved_off" "$now_size"
	cat "$1"
	error_exit 1
fi

# Grow the file back to the saved offset with bytes that are not messages,
# then append two new lines. A reader that stayed at the saved offset emits
# only those two lines. A reader that replayed from byte 0 also emits the
# surviving prefix, and seq_check rejects that. Waiting on the two lines
# also fails if the input never started.
pad=$((saved_off - now_size))
dd if=/dev/zero bs=1 count="$pad" status=none >> "$RSYSLOG_DYNNAME.input"
./inputfilegen -m 2 -i 100 >> "$RSYSLOG_DYNNAME.input"

rm -f "$RSYSLOG_OUT_LOG"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 2 50
shutdown_when_empty
wait_shutdown
seq_check 100 101
exit_test
