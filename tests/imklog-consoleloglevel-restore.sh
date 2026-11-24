#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that imklog restores the kernel console log level on shutdown.
# consoleLogLevel changes the console log level at startup; on shutdown
# imklog must set it back to the level it found. klogctl() is replaced by
# a preload library that only records the calls, so the test needs no
# privileges and does not change the host's console log level. The level
# in effect before startup is read from /proc/sys/kernel/printk, the same
# source imklog uses.
# Oracle: after shutdown the recorded calls are exactly "set level to 3"
# followed by "set level to the previous value" (klogctl action 8).
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin imklog
if [ ! -r /proc/sys/kernel/printk ]; then
	echo "SKIP: /proc/sys/kernel/printk is not readable"
	skip_test
fi
orig_level=$(awk '{print $1; exit}' /proc/sys/kernel/printk)
export RSYSLOG_KLOGCTL_EVENTS="$PWD/$RSYSLOG_DYNNAME.klogctl"
export RSYSLOG_PRELOAD="./.libs/libimklog_klogctl_preload.so"
printf 'msgnum:1\n' > "$RSYSLOG_DYNNAME.kmsg"
generate_conf
add_conf '
module(load="../plugins/imklog/.libs/imklog" logpath="'$RSYSLOG_DYNNAME.kmsg'"
       consoleLogLevel="3")

template(name="outfmt" type="string" string="%msg%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
export EXPECTED="klogctl 8 3
klogctl 8 $orig_level"
cmp_exact "$RSYSLOG_KLOGCTL_EVENTS"
exit_test
