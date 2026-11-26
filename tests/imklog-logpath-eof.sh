#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that imklog does not busy-loop when its LogPath reports end of file.
# LogPath points to a regular file, so every read() after the file content
# was consumed returns 0. imklog must deliver the lines, log one warning
# about the end of file and then poll, instead of retrying immediately.
# Wait for the warning before appending a fourth line, then require the exact
# output sequence and one warning. On Linux, measure CPU before appending:
# a busy loop uses close to 3 s of CPU in a 3 s window, while an idle reader
# uses almost nothing. A 1 s limit leaves room for testbench activity.
. ${srcdir:=.}/diag.sh init
# imklog has no supported Solaris driver; use imsolaris for kernel input.
skip_platform "SunOS" "imklog is not supported on Solaris"
require_plugin imklog
printf 'msgnum:1\nmsgnum:2\nmsgnum:3\n' > "$RSYSLOG_DYNNAME.kmsg"
generate_conf
if [ "${IMKLOG_LOGPATH_YAML:-}" = YES ]; then
	require_yaml_support
	cat > "$RSYSLOG_DYNNAME.imklog.yaml" <<EOF
modules:
  - load: "../plugins/imklog/.libs/imklog"
    logpath: "$RSYSLOG_DYNNAME.kmsg"
EOF
	add_conf 'include(file="'${RSYSLOG_DYNNAME}'.imklog.yaml")'
else
	add_conf 'module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.kmsg'")'
fi
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 3
wait_content "end of file reached on kernel log" "$RSYSLOG_DYNNAME.started"

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
else
	echo "Skipping CPU measurement: /proc/$pid/stat is unreadable"
fi

printf 'msgnum:4\n' >> "$RSYSLOG_DYNNAME.kmsg"
wait_content "msgnum:4" "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
export EXPECTED='msgnum:1
msgnum:2
msgnum:3
msgnum:4'
cmp_exact
content_count_check "end of file reached on kernel log" 1 "$RSYSLOG_DYNNAME.started"
exit_test
