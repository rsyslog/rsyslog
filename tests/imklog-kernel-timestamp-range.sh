#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that imklog only uses a kernel timestamp "[secs.usecs]" as message
# time when its value is plausible for the running system. On kernels that
# print no timestamps, the bracketed text can come from a user-chosen process
# name, so out-of-range values must not become the message time.
#
# Setup: LogPath regular file, parsekerneltimestamp="on". Line 1 carries a
# valid timestamp (1 s after boot). Lines 2-5 are syntactically valid but out
# of range: too many seconds digits (12 and 17, which used to overflow or wrap
# the year), a value later than the current uptime, and more than six
# microsecond digits.
# Oracle: for an implausible timestamp imklog passes no time, so
# timereported is the reception time and must lie between the wall clock
# before startup and after shutdown (1 s slack for second granularity). Note
# that timegenerated cannot serve as reference: imklog sets it from the kernel
# timestamp too. For the valid line, timereported must be boot time + 1 s
# (boot time from /proc/uptime); the 10 s tolerance covers second-granular
# values and testbench delays. In all lines the
# timestamp text must be stripped as before, and the weekday name must be a
# real weekday.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
if [ ! -r /proc/uptime ]; then
	echo "SKIP: /proc/uptime is not readable"
	skip_test
fi
cat > "$RSYSLOG_DYNNAME.kmsg" <<'KMSG'
<6>[    1.000000] msgnum:1 valid
<6>[975000000000.] msgnum:2 twelve second digits
<6>[99999999999999999.0] msgnum:3 seventeen second digits
<6>[999999999.000000] msgnum:4 later than uptime
<6>[    1.0000001] msgnum:5 seven usec digits
KMSG
generate_conf
add_conf 'module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.kmsg'"
	parsekerneltimestamp="on")
template(name="outfmt" type="string"
	string="%timereported:::date-unixtimestamp% %timereported:::date-wdayname% %msg%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
t_start=$(date +%s)
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 5
shutdown_when_empty
wait_shutdown
t_end=$(date +%s)

boot=$(( $(date +%s) - $(cut -d. -f1 /proc/uptime) ))
cat "$RSYSLOG_OUT_LOG"
while read -r reported wday msg; do
	case "$wday" in
	Sun|Mon|Tue|Wed|Thu|Fri|Sat) ;;
	*) echo "FAIL: invalid weekday name '$wday' for: $msg"; error_exit 1 ;;
	esac
	case "$msg" in
	*'['*) echo "FAIL: kernel timestamp not stripped: $msg"; error_exit 1 ;;
	esac
	case "$msg" in
	"msgnum:1 "*)
		delta=$(( reported - boot - 1 ))
		if [ "$delta" -lt -10 ] || [ "$delta" -gt 10 ]; then
			echo "FAIL: valid kernel timestamp not used: reported=$reported boot=$boot"
			error_exit 1
		fi
		;;
	*)
		if [ "$reported" -lt $((t_start - 1)) ] || [ "$reported" -gt $((t_end + 1)) ]; then
			echo "FAIL: implausible kernel timestamp used as message time: reported=$reported" \
				"expected $t_start..$t_end: $msg"
			error_exit 1
		fi
		;;
	esac
done < "$RSYSLOG_OUT_LOG"
exit_test
