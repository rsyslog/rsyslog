#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Same setup as imfile-truncate-state-past-eof.sh, with the first fstat() of
# the empty-buffer truncation check failing. The failure must be logged and
# ignored: the next poll retries the check, detects the truncation and reads
# the surviving prefix from byte 0.
. ${srcdir:=.}/diag.sh init
platform="$(uname)"
if [ "$platform" != "Linux" ]; then
	echo "platform is \"$platform\" - test requires /proc/self/fd and LD_PRELOAD support"
	skip_test
fi
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
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

export RSYSLOG_PRELOAD=.libs/liboverride_fstat.so
export RSYSLOG_TEST_FSTAT_FAIL_SUFFIX="/$RSYSLOG_DYNNAME.input"
rm -f "$RSYSLOG_OUT_LOG"
startup
wait_content "cannot fstat() for truncation check" "$RSYSLOG_DYNNAME.started"
wait_file_lines "$RSYSLOG_OUT_LOG" "$prefix_lines" 50
shutdown_when_empty
wait_shutdown
content_count_check "cannot fstat() for truncation check" 1 "$RSYSLOG_DYNNAME.started"
seq_check 0 $((prefix_lines - 1))
exit_test
