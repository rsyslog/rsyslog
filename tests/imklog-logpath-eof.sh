#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that imklog does not busy-loop when its LogPath reports end of file.
# LogPath points to a regular file, so every read() after the file content
# was consumed returns 0. imklog must deliver the lines, log one warning
# about the end of file and then poll, instead of retrying immediately.
# Oracle: all lines arrive, the warning is logged exactly once, and on
# Linux rsyslogd uses less than 1 s of CPU time during a 3 s window after
# the end of file. A busy loop uses close to the whole window; an idle
# rsyslogd uses almost nothing, so the threshold is safe on loaded runners.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
printf 'msgnum:1\nmsgnum:2\nmsgnum:3\n' > "$RSYSLOG_DYNNAME.kmsg"
generate_conf
add_conf '
module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.kmsg'")

template(name="outfmt" type="string" string="%msg%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 3

pid=$(getpid)
if [ -r "/proc/$pid/stat" ]; then
	clk_tck=$(getconf CLK_TCK)
	# fields 14 and 15 of /proc/<pid>/stat are utime and stime in clock ticks
	cpu_before=$(awk '{print $14 + $15}' "/proc/$pid/stat")
	sleep 3 # measurement window, see header comment
	cpu_after=$(awk '{print $14 + $15}' "/proc/$pid/stat")
	cpu_used=$((cpu_after - cpu_before))
	echo "rsyslogd used $cpu_used clock ticks in 3 s (CLK_TCK=$clk_tck)"
	if [ "$cpu_used" -ge "$clk_tck" ]; then
		echo "FAIL: imklog seems to busy-loop at end of file"
		error_exit 1
	fi
fi
wait_content "end of file reached on kernel log" "$RSYSLOG_DYNNAME.started"

shutdown_when_empty
wait_shutdown
export EXPECTED='msgnum:1
msgnum:2
msgnum:3'
cmp_exact
content_count_check "end of file reached on kernel log" 1 "$RSYSLOG_DYNNAME.started"
exit_test
