#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Verify that mmaitag's configurable provider budgets remain bounded and
# internally consistent. The oracle is rsyslogd -N1 rejecting both an
# excessive response allowance and a connect timeout longer than the total
# request timeout; no provider connection or timing threshold is involved.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmaitag/.libs/mmaitag")
action(type="mmaitag" provider="gemini_mock" apikey="dummy"
       response.maxBytes="67108865")
'

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.size.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected response.maxBytes validation failure"
    cat "${RSYSLOG_DYNNAME}.size.log"
    error_exit 1
fi
content_check "mmaitag: response.maxBytes must not exceed 67108864" "${RSYSLOG_DYNNAME}.size.log"

generate_conf 2
add_conf '
module(load="../plugins/mmaitag/.libs/mmaitag")
action(type="mmaitag" provider="gemini_mock" apikey="dummy"
       request.timeoutMs="1000" request.connectTimeoutMs="1001")
' 2

../tools/rsyslogd -N1 -f"${TESTCONF_NM}2.conf" -M../runtime/.libs:../.libs >"${RSYSLOG_DYNNAME}.timeout.log" 2>&1
if [ $? -ne 1 ]; then
    echo "FAIL: expected connect-timeout validation failure"
    cat "${RSYSLOG_DYNNAME}.timeout.log"
    error_exit 1
fi
content_check "mmaitag: request.connectTimeoutMs must not exceed request.timeoutMs" \
    "${RSYSLOG_DYNNAME}.timeout.log"

exit_test
