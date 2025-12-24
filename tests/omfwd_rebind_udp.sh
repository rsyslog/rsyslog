#!/bin/bash
# This test reproduces the issue where RebindInterval causes action suspension
# added 2024-12-24 by Antigravity (Rgerhards). Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
module(load="builtin:omfwd")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

# Use a very small rebind interval to trigger it quickly
# Use a small pool.resumeinterval to avoid long wait, but still detectable
action(type="omfwd" target="127.0.0.1" port="'$PORT_RCVR'" protocol="udp"
       RebindInterval="2" pool.resumeinterval="2")
'
# We need a receiver to avoid other types of errors, though UDP doesn't care much
# but having one makes it cleaner. We'll use a simple nc or just let it be.
# actually rsyslog tests often use a second rsyslog instance as receiver.

# Enable debug logging to capture suspension messages
export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="${RSYSLOG_DYNNAME}.debuglog"

startup

# Send 5 messages.
# Message 1: OK
# Message 2: OK, but after this, RebindInterval=2 is reached.
# Transaction commit will call DestructTCPTargetData -> ttResume = now + 2s.
# Message 3: will try to send, but action will be suspended for 2s.
injectmsg 0 5

shutdown_when_empty
wait_shutdown

# Check the logs for the rebind message
content_check "omfwd: dropped connections due to configured rebind interval" "$RSYSLOG_DEBUGLOG"

# VERIFY: No suspension should occur now
if grep -q "suspending action" "$RSYSLOG_DEBUGLOG"; then
    echo "FAIL: Action was suspended during rebind!"
    cat "$RSYSLOG_DEBUGLOG"
    exit 1
fi

echo "SUCCESS: No suspension detected during rebind."

exit_test
