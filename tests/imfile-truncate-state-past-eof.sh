#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# A same-inode shrink that happens while imfile is stopped leaves the saved
# offset past EOF. With reopenOnTruncate on, the next start reads the
# surviving prefix from byte 0.
. ${srcdir:=.}/diag.sh init
check_command_available truncate

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="polling" PollingInterval="1")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="file:" reopenOnTruncate="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

# 80 lines is past the 512-byte file-id window, so the state file name stays
# valid after an in-place truncate that keeps the beginning.
./inputfilegen -m 80 > "$RSYSLOG_DYNNAME.input"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 80 50
shutdown_when_empty
wait_shutdown
seq_check 0 79

prefix_info=$(awk '
	{ total += length($0) + 1; lines++
	  if (total >= 512) { print total, lines; exit }
	}
' "$RSYSLOG_DYNNAME.input")
prefix_bytes=${prefix_info%% *}
prefix_lines=${prefix_info##* }
if [ -z "$prefix_bytes" ] || [ "$prefix_bytes" -lt 512 ]; then
	printf 'FAIL: input shorter than the file-id window (%s)\n' "$prefix_info"
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

rm -f "$RSYSLOG_OUT_LOG"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" "$prefix_lines" 50
shutdown_when_empty
wait_shutdown
seq_check 0 $((prefix_lines - 1))
exit_test
