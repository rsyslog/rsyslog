#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# A live imtcp listener on a kernel-assigned loopback port must reject a second
# listener. Only inputStartupPolicy=require-all may abort startup. Also verify
# that the policy rejects a missing input module, the port-0 fallback, and a
# missing ruleset that would otherwise route messages to the default ruleset.
# A ten-second bound detects a strict-mode hang; surviving three seconds
# proves the compatibility cases stayed running. The port file proves that the
# holder is listening before each conflicting rsyslogd starts.
# A two-second termination grace is followed by SIGKILL if a daemon hangs.
# Test both RainerScript and YAML when the YAML frontend is built. Diagnostics
# are read from process output because activation fails before input delivery.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

root=$(cd .. && pwd)
testdir=$PWD
daemon="$root/tools/rsyslogd"
modules="$root/plugins/imtcp/.libs:$root/plugins/omfile/.libs:$root/runtime/.libs"
portfile="$testdir/$RSYSLOG_DYNNAME.holder.port"
holder_conf="$testdir/$RSYSLOG_DYNNAME.holder.conf"
holder_log="$testdir/$RSYSLOG_DYNNAME.holder.log"

cat > "$holder_conf" <<EOF
module(load="imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="$portfile")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.holder.out")
EOF
"$daemon" -n -iNONE -f "$holder_conf" -M "$modules" > "$holder_log" 2>&1 &
holder_pid=$!
cleanup_holder() {
	kill "$holder_pid" 2>/dev/null || :
	wait "$holder_pid" 2>/dev/null || :
}
trap cleanup_holder EXIT

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

wait_file_exists_for_process "$portfile" "$holder_pid" 10 "imtcp holder" "$holder_log"
port=$(cat "$portfile")
case "$port" in
	''|*[!0-9]*) error_exit 1 "invalid holder port: $port" ;;
esac

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
		if [ "$format" = yaml ]; then
			config="$testdir/$RSYSLOG_DYNNAME.$case_name.yaml"
			cat > "$config" <<EOF
version: 2
global:
  inputStartupPolicy: "$policy"
modules:
  - load: imtcp
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "$port"
rulesets:
  - name: main
    script: |
      action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
		else
			config="$testdir/$RSYSLOG_DYNNAME.$case_name.conf"
			cat > "$config" <<EOF
global(inputStartupPolicy="$policy")
module(load="imtcp")
input(type="imtcp" address="127.0.0.1" port="$port")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
		fi
		if [ "$should_exit" = yes ]; then
			run_bounded 10 "$config" "$log"
			status=$?
			if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
				cat "$log"
				error_exit 1 "$format $case_name did not fail startup promptly (status $status)"
			fi
			grep -Fq 'activation of module imtcp failed' "$log" || error_exit 1 "$format $case_name missing activation error"
		else
			run_bounded 3 "$config" "$log"
			status=$?
			if [ "$status" -ne 124 ]; then
				cat "$log"
				error_exit 1 "$format $case_name exited unexpectedly (status $status)"
			fi
		fi
		grep -Fq 'Could not create tcp listener' "$log" || error_exit 1 "$format $case_name missing bind diagnostic"
		grep -Fq "TCP port $port" "$log" || error_exit 1 "$format $case_name missing port diagnostic"
		if grep -Fq 'one visible listener is PID' "$log"; then
			grep -Fq "one visible listener is PID $holder_pid" "$log" || error_exit 1 "$format $case_name wrong owner PID"
		else
			grep -Fq 'listener owner unavailable' "$log" || error_exit 1 "$format $case_name missing owner fallback"
		fi
	done
done

# A missing input module must also be fatal under the global policy. The
# best-effort control must remain alive for the three-second observation bound.
for policy in best-effort require-all; do
	config="$testdir/$RSYSLOG_DYNNAME.missing-$policy.conf"
	log="$testdir/$RSYSLOG_DYNNAME.missing-$policy.log"
	cat > "$config" <<EOF
global(inputStartupPolicy="$policy")
module(load="imtcp-startup-policy-missing")
input(type="imtcp-startup-policy-missing" port="0")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
	if [ "$policy" = require-all ]; then
		run_bounded 10 "$config" "$log"
		status=$?
		if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
			cat "$log"
			error_exit 1 "missing input module did not fail strict startup (status $status)"
		fi
	else
		run_bounded 3 "$config" "$log"
		status=$?
		if [ "$status" -ne 124 ]; then
			cat "$log"
			error_exit 1 "missing input module stopped best-effort startup (status $status)"
		fi
	fi
	grep -Fq "input module name 'imtcp-startup-policy-missing' is unknown" "$log" ||
		error_exit 1 "missing input-module diagnostic"
done

# In strict mode, imtcp must not replace an unspecified ephemeral port with 514.
config="$testdir/$RSYSLOG_DYNNAME.port-zero.conf"
log="$testdir/$RSYSLOG_DYNNAME.port-zero.log"
cat > "$config" <<EOF
global(inputStartupPolicy="require-all")
module(load="imtcp")
input(type="imtcp" address="127.0.0.1" port="0")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
run_bounded 10 "$config" "$log"
status=$?
if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
	cat "$log"
	error_exit 1 "port zero without port file did not fail strict startup (status $status)"
fi
grep -Fq 'port 0 needs listenPortFileName' "$log" || error_exit 1 "missing port fallback diagnostic"

# A missing ruleset would route messages elsewhere in best-effort mode.
for policy in best-effort require-all; do
	config="$testdir/$RSYSLOG_DYNNAME.ruleset-$policy.conf"
	log="$testdir/$RSYSLOG_DYNNAME.ruleset-$policy.log"
	portfile="$testdir/$RSYSLOG_DYNNAME.ruleset-$policy.port"
	cat > "$config" <<EOF
global(inputStartupPolicy="$policy")
module(load="imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="$portfile" ruleset="missing")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
	if [ "$policy" = require-all ]; then
		run_bounded 10 "$config" "$log"
		status=$?
		if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
			cat "$log"
			error_exit 1 "missing ruleset did not fail strict startup (status $status)"
		fi
		grep -Fq "input-bound ruleset 'missing' not found" "$log" || error_exit 1 "missing strict ruleset diagnostic"
	else
		run_bounded 3 "$config" "$log"
		status=$?
		if [ "$status" -ne 124 ] || [ ! -s "$portfile" ]; then
			cat "$log"
			error_exit 1 "missing ruleset stopped best-effort startup (status $status)"
		fi
		grep -Fq 'using default ruleset instead' "$log" || error_exit 1 "missing fallback diagnostic"
	fi
done

exit_test
