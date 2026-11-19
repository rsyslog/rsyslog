#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that imklog stops reading after a read error on its LogPath and
# reports the error once instead of retrying on a closed descriptor.
# LogPath is /proc/self/mem: rsyslogd can open it and the zero-length read
# check at startup succeeds, but the first real read fails (EIO). imklog
# must log the read error once and terminate its input thread.
# Wait for the worker-exit diagnostic before asking rsyslogd to shut down.
# That proves the read failure ended the input thread. After shutdown, verify
# that the error occurred exactly once.
. ${srcdir:=.}/diag.sh init
require_plugin imklog
if [ "$(uname)" != "Linux" ] || [ ! -e /proc/self/mem ]; then
	echo "SKIP: test requires Linux /proc/self/mem"
	skip_test
fi
generate_conf
if [ "${IMKLOG_LOGPATH_YAML:-}" = YES ]; then
	require_yaml_support
	cat > "$RSYSLOG_DYNNAME.imklog.yaml" <<EOF
modules:
  - load: "../plugins/imklog/.libs/imklog"
    logpath: "/proc/self/mem"
EOF
	add_conf 'include(file="'${RSYSLOG_DYNNAME}'.imklog.yaml")'
else
	add_conf 'module(load="../plugins/imklog/.libs/imklog" logpath="/proc/self/mem")'
fi
startup
wait_content "imklog: [a-z ]*kernel log" "$RSYSLOG_DYNNAME.started"
if grep -q "cannot open kernel log" "$RSYSLOG_DYNNAME.started"; then
	shutdown_when_empty
	wait_shutdown
	echo "SKIP: /proc/self/mem cannot be used as LogPath on this system"
	skip_test
fi
wait_content "main thread of imklog terminated abnormally" "$RSYSLOG_DYNNAME.started"
shutdown_when_empty
wait_shutdown
content_count_check "imklog: error reading kernel log" 1 "$RSYSLOG_DYNNAME.started"
exit_test
