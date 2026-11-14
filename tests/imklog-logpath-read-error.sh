#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that imklog stops reading after a read error on its LogPath and
# reports the error once instead of retrying on a closed descriptor.
# LogPath is /proc/self/mem: rsyslogd can open it and the zero-length read
# check at startup succeeds, but the first real read fails (EIO). imklog
# must log the read error once and terminate its input thread.
# Oracle: after the error was logged, wait 2 s (time for a retry loop to
# show up) and then require that the error message is present exactly once,
# that the input thread ended, and that rsyslogd shuts down cleanly.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
if [ "$(uname)" != "Linux" ] || [ ! -e /proc/self/mem ]; then
	echo "SKIP: test requires Linux /proc/self/mem"
	skip_test
fi
generate_conf
add_conf '
module(load="../plugins/imklog/.libs/imklog" logpath="/proc/self/mem")
'
startup
wait_content "imklog: [a-z ]*kernel log" "$RSYSLOG_DYNNAME.started"
if grep -q "cannot open kernel log" "$RSYSLOG_DYNNAME.started"; then
	shutdown_when_empty
	wait_shutdown
	echo "SKIP: /proc/self/mem cannot be used as LogPath on this system"
	skip_test
fi
sleep 2 # give a retry loop time to repeat the message, see header comment
shutdown_when_empty
wait_shutdown
content_count_check "imklog: error reading kernel log" 1 "$RSYSLOG_DYNNAME.started"
content_check "main thread of imklog terminated abnormally" "$RSYSLOG_DYNNAME.started"
exit_test
