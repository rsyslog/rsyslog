#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Verify that imuxsock validates listener socket paths before touching the
# filesystem:
#  1. a regular file at a Socket= path must not be removed (Unlink defaults to
#     "on"); creating the listener fails instead and a diagnostic is emitted;
#  2. a Socket= path longer than sockaddr_un.sun_path is rejected instead of
#     being bound under a silently truncated name.
# Oracles: the regular file still exists with its original content, no socket
# with the truncated name was created, and both diagnostics reach the default
# testbench output for internal messages. Shutdown is synchronized before the
# checks so the diagnostics have been written.
. ${srcdir:=.}/diag.sh init

# Both listeners intentionally fail to activate; the module activation failure
# path can leave setup allocations for process teardown.
export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

regular_file="$RSYSLOG_DYNNAME.not-a-socket"
printf 'keep me\n' >"$regular_file"

# 200 bytes is longer than sun_path on all supported platforms (104-108 bytes)
# while still being a valid single path component (<= 255 bytes).
long_prefix="$RSYSLOG_DYNNAME.long-"
long_path="$long_prefix$(printf 'x%.0s' $(seq 1 200))"
long_path="${long_path:0:200}"

generate_conf
add_conf '
module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
input(type="imuxsock" Socket="'$regular_file'")
input(type="imuxsock" Socket="'$long_path'")
'

startup
shutdown_when_empty
wait_shutdown

if [ ! -f "$regular_file" ] || [ "$(cat "$regular_file")" != "keep me" ]; then
	echo "FAIL: regular file at imuxsock Socket path was removed or modified"
	error_exit 1
fi
content_check "imuxsock: not removing '$regular_file': path exists and is not a socket" "$RSYSLOG_DYNNAME.started"

if ls "$long_prefix"* >/dev/null 2>&1; then
	echo "FAIL: socket created under a truncated name for an overlong Socket path"
	ls -l "$long_prefix"*
	error_exit 1
fi
content_check "cannot create '$long_prefix" "$RSYSLOG_DYNNAME.started"

exit_test
