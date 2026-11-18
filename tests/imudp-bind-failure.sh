#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# A helper-owned UDP socket on a kernel-assigned loopback port must reject an
# imudp listener. Only inputStartupPolicy=require-all may abort startup. When
# IPv6 is available, a successful IPv6 bind must not hide the failed IPv4 bind.
# A ten-second bound detects a strict-mode hang; surviving three seconds
# proves the compatibility cases stayed running. The port file proves that the
# holder is listening before each conflicting rsyslogd starts.
# A two-second termination grace is followed by SIGKILL if a daemon hangs.
# Test both RainerScript and YAML when the YAML frontend is built. Diagnostics
# are read from process output because activation fails before input delivery.
. ${srcdir:=.}/diag.sh init
require_plugin imudp
check_command_available python3

root=$(cd .. && pwd)
testdir=$PWD
daemon="$root/tools/rsyslogd"
modules="$root/plugins/imudp/.libs:$root/plugins/omfile/.libs:$root/runtime/.libs"
portfile="$testdir/$RSYSLOG_DYNNAME.holder.port"
holder_log="$testdir/$RSYSLOG_DYNNAME.holder.log"

python3 -u -c '
import signal
import socket
import sys

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", 0))
with open(sys.argv[1], "w") as port_file:
    port_file.write(str(sock.getsockname()[1]))
while True:
    signal.pause()
' "$portfile" > "$holder_log" 2>&1 &
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

wait_file_exists_for_process "$portfile" "$holder_pid" 10 "imudp holder" "$holder_log"
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
  - load: imudp
inputs:
  - type: imudp
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
module(load="imudp")
input(type="imudp" address="127.0.0.1" port="$port")
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
			grep -Fq 'activation of module imudp failed' "$log" || error_exit 1 "$format $case_name missing activation error"
		else
			run_bounded 3 "$config" "$log"
			status=$?
			if [ "$status" -ne 124 ]; then
				cat "$log"
				error_exit 1 "$format $case_name exited unexpectedly (status $status)"
			fi
		fi
		grep -Fq 'Could not create udp listener' "$log" || error_exit 1 "$format $case_name missing bind diagnostic"

	done
done

# When both wildcard address families are usable, one failed IPv4 bind and one
# healthy IPv6 bind must still reject the whole required input. The probe
# avoids assuming that every CI runner has IPv6 enabled.
if python3 - <<'PY'
import socket

addresses = socket.getaddrinfo(None, "0", socket.AF_UNSPEC, socket.SOCK_DGRAM,
                               0, socket.AI_PASSIVE | socket.AI_NUMERICSERV)
if not {socket.AF_INET, socket.AF_INET6}.issubset({item[0] for item in addresses}):
    raise SystemExit(1)
with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as sock:
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
    sock.bind(("::", 0))
PY
then
	config="$testdir/$RSYSLOG_DYNNAME.partial-bind.conf"
	log="$testdir/$RSYSLOG_DYNNAME.partial-bind.log"
	cat > "$config" <<EOF
global(inputStartupPolicy="require-all")
module(load="imudp")
input(type="imudp" address="*" port="$port")
action(type="omfile" file="$testdir/$RSYSLOG_DYNNAME.out")
EOF
	run_bounded 10 "$config" "$log"
	status=$?
	if [ "$status" -eq 0 ] || [ "$status" -ge 124 ]; then
		cat "$log"
		error_exit 1 "partial UDP bind did not fail strict startup (status $status)"
	fi
	grep -Fq 'UDP listener startup requires all resolved addresses' "$log" || error_exit 1 "missing partial-bind diagnostic"
fi

exit_test
