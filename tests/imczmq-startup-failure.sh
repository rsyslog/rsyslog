#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# An IPC endpoint with a missing parent makes imczmq fail inside runInput.
# Strict policy must exit nonzero without a crash; best-effort must stay up.
# The three-second bound distinguishes continued service from startup exit.
. ${srcdir:=.}/diag.sh init
[ -f ../contrib/imczmq/.libs/imczmq.so ] || skip_test "imczmq not built"

root=$(cd .. && pwd)
modules="$root/contrib/imczmq/.libs:$root/plugins/omfile/.libs:$root/runtime/.libs"
for policy in best-effort require-all; do
	conf="$PWD/$RSYSLOG_DYNNAME.$policy.conf"
	log="$PWD/$RSYSLOG_DYNNAME.$policy.log"
	cat > "$conf" <<EOF
global(inputStartupPolicy="$policy")
module(load="imczmq")
input(type="imczmq" endpoints="ipc://$PWD/nonexistent-$RSYSLOG_DYNNAME/socket" socktype="PULL")
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
	grep -Fq 'zsock_attach to' "$log" || error_exit 1 "missing imczmq bind diagnostic"
	if [ "$policy" = require-all ]; then
		[ "$status" -eq 1 ] || error_exit 1 "strict imczmq startup returned $status, expected 1"
	else
		[ "$status" -eq 124 ] || error_exit 1 "best-effort imczmq exited early with $status"
	fi
done
exit_test
