#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Shared helper for config-load-abort.sh and yaml-config-load-abort.sh.
#
# cla_run_rsyslogd OUTFILE PIDFILE CONFFILE
# Starts rsyslogd in the foreground (normal start, not -N) and returns its
# exit code. The load is expected to abort at once. The wait is bounded only
# so that a regression which lets the daemon start does not hang the test:
# after TB_TEST_TIMEOUT seconds the daemon is killed and 124 is returned.
# (The coreutils "timeout" command is not available on all CI platforms,
# e.g. macOS.)
cla_run_rsyslogd() {
	local out="$1" pidf="$2" conf="$3" pid i limit
	../tools/rsyslogd -n -C -f"$conf" -M"$RSYSLOG_MODDIR" -i"$pidf" > "$out" 2>&1 &
	pid=$!
	limit=$(( ${TB_TEST_TIMEOUT:-60} * 10 ))
	for (( i = 0; i < limit; i++ )); do
		kill -0 "$pid" 2>/dev/null || break
		$TESTTOOL_DIR/msleep 100
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill -9 "$pid" 2>/dev/null
		wait "$pid" 2>/dev/null
		return 124
	fi
	wait "$pid"
}
