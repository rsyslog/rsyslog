#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML parity for the enabled imtcp limiter in omfile-legacy-dynafile-programname.sh.
# Send two messages on one connection to preserve admission order. The first
# must reach the action and the second must trigger a first-drop diagnostic
# identifying its host and APP-NAME. Wait for both configured outputs before
# shutdown to ensure imtcp has read both messages, then check for any leak.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
sed -e "s|@DYNNAME@|${RSYSLOG_DYNNAME}|g" -e "s|@OUT_LOG@|${RSYSLOG_OUT_LOG}|g" \
    "${srcdir}/testsuites/imtcp-ratelimit-first-drop.yaml" > "${TESTCONF_NM}.yaml"
add_conf '
include(file="'"${TESTCONF_NM}"'.yaml")
'

printf '%s\n' \
    '<14>1 2026-01-01T00:00:00Z myhost limitedapp - - - first limited' \
    '<14>1 2026-01-01T00:00:00Z myhost droppedapp - - - second limited' > "${RSYSLOG_DYNNAME}.limited.input"

startup
assign_tcpflood_port "${RSYSLOG_DYNNAME}.tcpflood_port"
tcpflood -I "${RSYSLOG_DYNNAME}.limited.input"
drop_diagnostic='from <myhost:droppedapp>: begin to drop messages due to rate-limiting'
wait_content 'first limited' "${RSYSLOG_OUT_LOG}"
wait_content "$drop_diagnostic" "${RSYSLOG_DYNNAME}.started"
shutdown_when_empty
wait_shutdown

content_check 'first limited' "${RSYSLOG_OUT_LOG}"
content_check "$drop_diagnostic" "${RSYSLOG_DYNNAME}.started"
check_not_present 'second limited' "${RSYSLOG_OUT_LOG}"
exit_test
