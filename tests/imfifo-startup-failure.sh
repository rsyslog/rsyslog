#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# A missing configured FIFO means the input loses a source at startup.
# Strict policy must exit 1, while best-effort keeps the process running.
# A three-second bound is the oracle for continued best-effort service.
. ${srcdir:=.}/diag.sh init
require_plugin imfifo

root=$(cd .. && pwd)
modules="$root/plugins/imfifo/.libs:$root/plugins/omfile/.libs:$root/runtime/.libs"
for policy in best-effort require-all; do
	conf="$PWD/$RSYSLOG_DYNNAME.$policy.conf"
	log="$PWD/$RSYSLOG_DYNNAME.$policy.log"
	cat > "$conf" <<EOF
global(inputStartupPolicy="$policy")
module(load="imfifo")
input(type="imfifo" file="$PWD/$RSYSLOG_DYNNAME.missing.fifo" tag="fifo:")
action(type="omfile" file="$PWD/$RSYSLOG_DYNNAME.out")
EOF
	"$root/tools/rsyslogd" -n -iNONE -f "$conf" -M "$modules" > "$log" 2>&1 &
	pid=$!
	status=0
	for ((seconds = 0; seconds < 3; seconds++)); do
		if ! kill -0 "$pid" 2>/dev/null; then
			wait "$pid"
			status=$?
			break
		fi
		sleep 1
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill "$pid" 2>/dev/null || :
		wait "$pid" 2>/dev/null || :
		status=124
	fi
	grep -Fq 'could not be opened' "$log" || error_exit 1 "missing FIFO diagnostic"
	if [ "$policy" = require-all ]; then
		[ "$status" -eq 1 ] || error_exit 1 "strict imfifo startup returned $status, expected 1"
	else
		[ "$status" -eq 124 ] || error_exit 1 "best-effort imfifo exited early with $status"
	fi
done
exit_test
