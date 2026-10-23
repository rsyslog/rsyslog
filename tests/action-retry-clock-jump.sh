#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression for issue #5245: a suspended omfwd action must retry after its
# configured interval even when the wall clock moves 30 days backward.
# The receiver owns its port before rsyslog starts, but listens only after
# suspension is observed in the action debug log. An action with no queued
# work does not wake itself, so a second message after 11 real seconds drives
# the retry. Delivery within 10 more seconds proves recovery. The 10-second
# interval leaves time to make the jump before the old deadline expires.
# The preflight checks that this libfaketime can move wall time without moving
# CLOCK_MONOTONIC; older versions fake both clocks and cannot model the bug.
# RSYSLOG_ACTION_RETRY_CLOCK_SHIFT=none is a local control of the same path.
. ${srcdir:=.}/diag.sh init
. $srcdir/faketime_common.sh

export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
export FAKETIME_TIMESTAMP_FILE="$PWD/$RSYSLOG_DYNNAME.faketime"
export FAKETIME_NO_CACHE=1
export FAKETIME_DONT_FAKE_MONOTONIC=1
printf '+0\n' > "$FAKETIME_TIMESTAMP_FILE"

if ! command -v python3 >/dev/null 2>&1; then
    echo 'python3 unavailable; cannot check libfaketime clock isolation'
    exit 77
fi

# Probe the same timestamp-file update used below. A wall jump must take
# effect, while monotonic time must not move backward. This is a capability
# check, not the retry oracle; the delivery check below remains the oracle.
LD_PRELOAD="$RSYSLOG_LIBFAKETIME" python3 - "$FAKETIME_TIMESTAMP_FILE" <<'PY'
import ctypes
import pathlib
import sys
import time


class Timeval(ctypes.Structure):
    _fields_ = [('tv_sec', ctypes.c_long), ('tv_usec', ctypes.c_long)]

class Timespec(ctypes.Structure):
    _fields_ = [('tv_sec', ctypes.c_long), ('tv_nsec', ctypes.c_long)]


libc = ctypes.CDLL(None)
libc.gettimeofday.argtypes = (ctypes.POINTER(Timeval), ctypes.c_void_p)
libc.clock_gettime.argtypes = (ctypes.c_int, ctypes.POINTER(Timespec))


def clocks():
    wall = Timeval()
    mono = Timespec()
    if libc.gettimeofday(ctypes.byref(wall), None) != 0:
        raise OSError('gettimeofday failed')
    if libc.clock_gettime(time.CLOCK_MONOTONIC, ctypes.byref(mono)) != 0:
        raise OSError('clock_gettime(CLOCK_MONOTONIC) failed')
    return wall.tv_sec + wall.tv_usec / 1e6, mono.tv_sec + mono.tv_nsec / 1e9


wall_before, mono_before = clocks()
pathlib.Path(sys.argv[1]).write_text('-30d\n')
wall_after, mono_after = clocks()
if wall_after >= wall_before - 29 * 86400:
    print('libfaketime did not apply the backward wall-clock jump')
    sys.exit(77)
if mono_after < mono_before:
    print('libfaketime also moved CLOCK_MONOTONIC backward')
    sys.exit(77)
PY
probe_rc=$?
printf '+0\n' > "$FAKETIME_TIMESTAMP_FILE"
if [ "$probe_rc" -eq 77 ]; then
    exit 77
elif [ "$probe_rc" -ne 0 ]; then
    error_exit 1
fi

LISTEN_RELEASE="$RSYSLOG_DYNNAME.minitcpsrvr.listen"
LISTEN_READY="$RSYSLOG_DYNNAME.minitcpsrvr.ready"
rm -f "$LISTEN_RELEASE" "$LISTEN_READY"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1 "$LISTEN_RELEASE" "$LISTEN_READY"

generate_conf
add_conf '
global(debug.whitelist="on"
       debug.files=["action.c", "../action.c", "../../runtime/action.c", "../../../runtime/action.c"])
template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then {
    action(name="forwarder" type="omfwd" target="127.0.0.1"
           port="'$MINITCPSRVR_PORT1'" protocol="tcp" template="outfmt"
           queue.type="LinkedList" queue.dequeueBatchSize="1"
           action.resumeRetryCount="0" action.resumeInterval="10")
}
'
startup
injectmsg 0 1
wait_content "action 'forwarder' suspended" "$RSYSLOG_DEBUGLOG"

if [ "${RSYSLOG_ACTION_RETRY_CLOCK_SHIFT:-backward}" = backward ]; then
    printf '%s\n' '-30d' > "$FAKETIME_TIMESTAMP_FILE.new"
    mv "$FAKETIME_TIMESTAMP_FILE.new" "$FAKETIME_TIMESTAMP_FILE"
fi
touch "$LISTEN_RELEASE"
wait_file_exists "$LISTEN_READY"
$TESTTOOL_DIR/msleep 11000
injectmsg 1 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1 10

shutdown_when_empty
wait_shutdown
content_check "msgnum:00000001" "$RSYSLOG_OUT_LOG"
exit_test
