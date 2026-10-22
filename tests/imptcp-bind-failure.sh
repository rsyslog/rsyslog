#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# A live imptcp listener on a kernel-assigned loopback port must reject a second
# listener. With a second healthy input, only inputStartupPolicy=require-all
# may abort startup after a partial bind failure.
# A thirty-second bound detects a strict-mode hang even under parallel CI load;
# surviving three seconds proves the compatibility cases stayed running.
# The port file proves that the
# holder has selected its port; a connection proves listen() completed.
# Delay holder startup by 1.5 seconds to model a loaded CI runner. The port
# file must still appear before the readiness bound expires.
# A two-second termination grace is followed by SIGKILL if a daemon hangs.
# Test both RainerScript and YAML when the YAML frontend is built. Diagnostics
# are read from process output because they occur before input delivery.
. ${srcdir:=.}/diag.sh init
require_plugin imptcp

root=$(cd .. && pwd)
testdir=$PWD
daemon="$root/tools/rsyslogd"
modules="$root/plugins/imptcp/.libs:$root/plugins/omfile/.libs:$root/runtime/.libs"
portfile="$testdir/$RSYSLOG_DYNNAME.holder.port"
holder_conf="$testdir/$RSYSLOG_DYNNAME.holder.conf"
holder_log="$testdir/$RSYSLOG_DYNNAME.holder.log"

cat > "$holder_conf" <<EOF
module(load="imptcp")
input(type="imptcp" address="127.0.0.1" port="0" listenPortFileName="$portfile")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.holder.out")
EOF
(sleep 1.5; exec "$daemon" -n -iNONE -f "$holder_conf" -M "$modules") > "$holder_log" 2>&1 &
holder_pid=$!
cleanup_holder() {
	kill "$holder_pid" 2>/dev/null || :
	for ((shutdown_wait = 0; shutdown_wait < 2; shutdown_wait++)); do
		if ! kill -0 "$holder_pid" 2>/dev/null; then
			break
		fi
		sleep 1
	done
	if kill -0 "$holder_pid" 2>/dev/null; then
		kill -KILL "$holder_pid" 2>/dev/null || :
	fi
	wait "$holder_pid" 2>/dev/null || :
}
trap cleanup_holder EXIT

# A dead helper must fail immediately with its specific diagnostic rather than
# consuming the readiness budget. This guards the negative path independently
# of the delayed but healthy holder above.
sh -c 'exit 7' &
exited_pid=$!
wait "$exited_pid" || :
early_exit_log="$testdir/$RSYSLOG_DYNNAME.early-exit.log"
if (error_exit() { exit "$1"; }; wait_file_exists_for_process \
	"$testdir/$RSYSLOG_DYNNAME.never.port" "$exited_pid" 200 \
	"exited helper" "") > "$early_exit_log" 2>&1; then
	error_exit 1 "readiness wait accepted an exited helper"
fi
if ! grep -q 'exited before writing readiness file' "$early_exit_log"; then
	error_exit 1 "readiness wait did not diagnose the exited helper"
fi

# Keep the startup bound portable to macOS, where GNU timeout is unavailable.
# Return 124 when rsyslogd is still running at the deadline, after stopping it.
run_bounded() {
	local seconds=$1
	local config=$2
	local log=$3
	local pid elapsed status
	"$daemon" -n -iNONE -f "$config" -M "$modules" > "$log" 2>&1 &
	pid=$!
	for ((elapsed = 0; elapsed < seconds; elapsed++)); do
		if ! kill -0 "$pid" 2>/dev/null; then
			wait "$pid"
			return $?
		fi
		sleep 1
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill "$pid" 2>/dev/null || :
		for ((elapsed = 0; elapsed < 2; elapsed++)); do
			if ! kill -0 "$pid" 2>/dev/null; then
				break
			fi
			sleep 1
		done
		if kill -0 "$pid" 2>/dev/null; then
			kill -KILL "$pid" 2>/dev/null || :
		fi
		wait "$pid" 2>/dev/null || :
		return 124
	fi
	wait "$pid"
	status=$?
	return "$status"
}

# The helper counts 100 ms polls; allow the standard 20-second startup budget.
wait_file_exists_for_process "$portfile" "$holder_pid" 200 "imptcp holder" "$holder_log"
port=$(cat "$portfile")
case "$port" in
	''|*[!0-9]*) error_exit 1 "invalid holder port: $port" ;;
esac
for ((ready = 0; ready < 10; ready++)); do
	if (echo > "/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
		break
	fi
	sleep 1
done
[ "$ready" -lt 10 ] || error_exit 1 "imptcp holder did not begin listening"

formats=rsyslog
if grep -q '^#define HAVE_LIBYAML 1$' "$root/config.h"; then
	formats='rsyslog yaml'
fi
for format in $formats; do
	for case_name in default strict; do
		case "$case_name" in
			default) policy=best-effort; should_exit=no ;;
			strict) policy=require-all; should_exit=yes ;;
		esac
		log="$testdir/$RSYSLOG_DYNNAME.$format.$case_name.log"
		ready_portfile="$testdir/$RSYSLOG_DYNNAME.$format.$case_name.ready.port"
		if [ "$format" = yaml ]; then
			config="$testdir/$RSYSLOG_DYNNAME.$case_name.yaml"
			cat > "$config" <<EOF
version: 2
global:
  inputStartupPolicy: "$policy"
modules:
  - load: imptcp
inputs:
  - type: imptcp
    address: "127.0.0.1"
    port: "$port"
  - type: imptcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "$ready_portfile"
rulesets:
  - name: main
    script: |
      action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
		else
			config="$testdir/$RSYSLOG_DYNNAME.$case_name.conf"
			cat > "$config" <<EOF
global(inputStartupPolicy="$policy")
module(load="imptcp")
input(type="imptcp" address="127.0.0.1" port="$port")
input(type="imptcp" address="127.0.0.1" port="0" listenPortFileName="$ready_portfile")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
		fi
		if [ "$should_exit" = yes ]; then
			run_bounded 30 "$config" "$log"
			status=$?
			if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
				cat "$log"
				error_exit 1 "$format $case_name did not fail startup promptly (status $status)"
			fi
			grep -Fq 'activation of module imptcp failed' "$log" || error_exit 1 "$format $case_name missing activation error"
		else
			run_bounded 3 "$config" "$log"
			status=$?
			if [ "$status" -ne 124 ]; then
				cat "$log"
				error_exit 1 "$format $case_name exited unexpectedly (status $status)"
			fi
			[ -s "$ready_portfile" ] || error_exit 1 "$format $case_name did not activate the healthy listener"
		fi
		grep -Fq 'imptcp: Error binding TCP port' "$log" || error_exit 1 "$format $case_name missing bind diagnostic"
		grep -Fq "TCP port $port" "$log" || error_exit 1 "$format $case_name missing port diagnostic"
		if grep -Fq 'one visible listener is PID' "$log"; then
			grep -Fq "one visible listener is PID $holder_pid" "$log" || error_exit 1 "$format $case_name wrong owner PID"
		else
			grep -Fq 'listener owner unavailable' "$log" || error_exit 1 "$format $case_name missing owner fallback"
		fi
	done
done

exit_test
