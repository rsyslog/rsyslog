#!/bin/bash
# added 2026-04-14 to guard issue #6547, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omrelp

export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
LISTENER_PORT_FILE="${RSYSLOG_DYNNAME}.holdtcpopen.port"
LISTENER_ACCEPT_FILE="${RSYSLOG_DYNNAME}.holdtcpopen.accepted"

$PYTHON ${srcdir}/holdtcpopen.py "${LISTENER_PORT_FILE}" "${LISTENER_ACCEPT_FILE}" 15 &
HOLDTCP_PID=$!
trap 'kill ${HOLDTCP_PID} 2>/dev/null || true' EXIT

wait_file_exists "${LISTENER_PORT_FILE}"
RELP_PORT=$(cat "${LISTENER_PORT_FILE}")

generate_conf
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")
main_queue(queue.type="Direct")
action(type="omrelp" target="127.0.0.1" port="'${RELP_PORT}'")
'

startup
injectmsg_literal "issue-6547"
wait_file_exists "${LISTENER_ACCEPT_FILE}"
shutdown_immediate
wait_shutdown

wait ${HOLDTCP_PID} || true
trap - EXIT
exit_test
