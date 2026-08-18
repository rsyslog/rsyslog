#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Check dynafile traversal when omfile uses its default output template. The
# blocked and valid messages share one input batch; the valid line must appear
# exactly once, proving the action core did not retry it after the drop.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

export TRAVERSAL_APP="${RSYSLOG_DYNNAME}-omfile-default-template-traversal"
export VALID_APP="${RSYSLOG_DYNNAME}-omfile-default-template-valid"
export TRAVERSAL_ROOT="${RSYSLOG_DYNNAME}.jail"
export TRAVERSAL_OUT="${TRAVERSAL_ROOT}/escape/${TRAVERSAL_APP}.log"
export VALID_OUT="${TRAVERSAL_ROOT}/base/goodhost/${VALID_APP}.log"
rm -rf "$TRAVERSAL_ROOT"
mkdir -p "${TRAVERSAL_ROOT}/base" "${TRAVERSAL_ROOT}/escape"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="dynfile" type="string" string="'$TRAVERSAL_ROOT'/base/%HOSTNAME%/%APP-NAME%.log")

$rulesetparser rsyslog.rfc5424
local4.debug action(type="omfile" dynafile="dynfile")
'

startup
{
	printf '<167>1 2003-03-01T01:00:00.000Z ../escape %s - - - traversal-default-template\n' \
		"$TRAVERSAL_APP"
	printf '<167>1 2003-03-01T01:00:00.000Z goodhost %s - - - valid-after-blocked-traversal\n' \
		"$VALID_APP"
} > "${RSYSLOG_DYNNAME}.input"
tcpflood -B -I "${RSYSLOG_DYNNAME}.input"
shutdown_when_empty
wait_shutdown

if [ -f "$TRAVERSAL_OUT" ]; then
	echo "FAIL: dynafile traversal with default output template created: $TRAVERSAL_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

if [ ! -f "$VALID_OUT" ]; then
	echo "FAIL: valid dynafile message after blocked traversal did not create: $VALID_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

if [ "$(grep -Fc 'valid-after-blocked-traversal' "$VALID_OUT")" -ne 1 ]; then
	echo "FAIL: valid dynafile record was retried after blocked traversal"
	cat "$VALID_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

rm -rf "$TRAVERSAL_ROOT"
exit_test
