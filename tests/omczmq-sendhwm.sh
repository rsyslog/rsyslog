#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Configured sendhwm must reach the real CZMQ socket before attachment. A shim
# observes getters on real sockets while bypassing attach/send, so no broker,
# connection deadline or queue-pressure timing is involved. Explicit nonzero,
# explicit unlimited (zero), INT_MAX, and an omitted-option control distinguish
# option application from overriding the CZMQ environment default (41). sndtimeo
# is an independent control.
# Drain and shutdown synchronize all observations before the exact comparison.
# The final -N1 controls assert process status for invalid numeric settings; no
# socket is created in validation mode, so invalid values cannot reach CZMQ.
. ${srcdir:=.}/diag.sh init
[ -f ../contrib/omczmq/.libs/omczmq.so ] || skip_test "omczmq not built"
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
export ZSYS_SNDHWM=41
export OMCZMQ_OPTIONS_LOG="$PWD/$RSYSLOG_DYNNAME.options"
export RSYSLOG_PRELOAD=.libs/libomczmq_socket_options_preload.so

generate_conf
add_conf '
module(load="../contrib/omczmq/.libs/omczmq")
if $msg contains "msgnum:" then {
    action(type="omczmq" socktype="PUSH" endpoints="inproc://configured"
        sendhwm="23" sendtimeout="17")
    action(type="omczmq" socktype="PUSH" endpoints="inproc://unlimited"
        sendhwm="0" sendtimeout="17")
    action(type="omczmq" socktype="PUSH" endpoints="inproc://default"
        sendtimeout="17")
    action(type="omczmq" socktype="PUSH" endpoints="inproc://maximum"
        sendhwm="2147483647" sendtimeout="17")
}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
export EXPECTED='inproc://configured sndhwm=23 sndtimeo=17
inproc://unlimited sndhwm=0 sndtimeo=17
inproc://default sndhwm=41 sndtimeo=17
inproc://maximum sndhwm=2147483647 sndtimeo=17'
cmp_exact "$OMCZMQ_OPTIONS_LOG"

for invalid in -1 2147483648; do
    cat > "$RSYSLOG_DYNNAME.invalid.conf" <<EOF
module(load="../contrib/omczmq/.libs/omczmq")
action(type="omczmq" socktype="PUSH" endpoints="inproc://invalid" sendhwm="$invalid")
EOF
    status=0
    ../tools/rsyslogd -N1 -f "$RSYSLOG_DYNNAME.invalid.conf" -M ../runtime/.libs \
        > "$RSYSLOG_DYNNAME.invalid-$invalid.log" 2>&1 || status=$?
    [ "$status" -eq 1 ] || error_exit 1 "sendhwm=$invalid returned $status, expected config error 1"
done
exit_test
