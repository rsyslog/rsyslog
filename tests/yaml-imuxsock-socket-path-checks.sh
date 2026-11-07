#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion for imuxsock_socket_path_checks.sh. The imuxsock module and
# its two Socket= inputs are loaded from a YAML include; the oracles are the
# same:
#  1. a regular file at a Socket path must survive (Unlink defaults to "on")
#     and a "not a socket" diagnostic must be emitted;
#  2. a Socket path longer than sockaddr_un.sun_path must be rejected without
#     creating a socket under a truncated name.
# Shutdown is synchronized before the checks so the internal diagnostics have
# reached the default testbench output.
. ${srcdir:=.}/diag.sh init
require_yaml_support

# Both listeners intentionally fail to activate; the module activation failure
# path can leave setup allocations for process teardown.
export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"

regular_file="$RSYSLOG_DYNNAME.not-a-socket"
printf 'keep me\n' >"$regular_file"

# 200 bytes exceeds sun_path on all supported platforms (104-108 bytes) while
# remaining a valid single path component (<= 255 bytes).
long_prefix="$RSYSLOG_DYNNAME.long-"
long_path="$long_prefix$(printf 'x%.0s' $(seq 1 200))"
long_path="${long_path:0:200}"

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat >"${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
modules:
  - load: "../plugins/imuxsock/.libs/imuxsock"
    sysSock.use: "off"

inputs:
  - type: imuxsock
    socket: "${regular_file}"
  - type: imuxsock
    socket: "${long_path}"
YAMLEOF

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
