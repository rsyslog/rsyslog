#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify that a negative imudp timeRequery value is rejected at config load,
# both as module parameter and as legacy $UDPServerTimeRequery. The value is
# used as a modulus in the receive loop, so it must never be applied. Zero
# remains valid (it means "query the time for every call").
. "${srcdir:=.}/diag.sh" init
require_plugin imudp

generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp" timeRequery="-1")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for timeRequery=-1"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "parameter 'timerequery' cannot be less than zero (was -1)" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected timeRequery validation error"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

generate_conf 2
add_conf '
$ModLoad ../plugins/imudp/.libs/imudp
$UDPServerTimeRequery -1
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
' 2

../tools/rsyslogd -N1 -f"${TESTCONF_NM}2.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.legacy.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected config validation failure for legacy UDPServerTimeRequery=-1"
    cat "${RSYSLOG_DYNNAME}.legacy.log"
    error_exit 1
fi

grep -F "value -1 must not be less than zero" "${RSYSLOG_DYNNAME}.legacy.log" >/dev/null || {
    echo "FAIL: expected legacy UDPServerTimeRequery validation error"
    cat "${RSYSLOG_DYNNAME}.legacy.log"
    error_exit 1
}

generate_conf 3
add_conf '
module(load="../plugins/imudp/.libs/imudp" timeRequery="0")
input(type="imudp" address="127.0.0.1" port="0")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
' 3

../tools/rsyslogd -N1 -f"${TESTCONF_NM}3.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.zero.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: timeRequery=0 must be accepted"
    cat "${RSYSLOG_DYNNAME}.zero.log"
    error_exit 1
fi

exit_test
