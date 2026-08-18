#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Check dynafile traversal when the dynafile template is a list template. A
# valid sibling message must be written once, proving the input was processed
# rather than simply dropped before the dynafile action.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

export TRAVERSAL_APP="${RSYSLOG_DYNNAME}-omfile-list-template-traversal"
export VALID_APP="${RSYSLOG_DYNNAME}-omfile-list-template-valid"
export TRAVERSAL_ROOT="${RSYSLOG_DYNNAME}.jail"
export TRAVERSAL_OUT="${TRAVERSAL_ROOT}/escape/${TRAVERSAL_APP}.log"
export VALID_OUT="${TRAVERSAL_ROOT}/base/goodhost/${VALID_APP}.log"
rm -rf "$TRAVERSAL_ROOT"
mkdir -p "${TRAVERSAL_ROOT}/base" "${TRAVERSAL_ROOT}/escape"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="dynfile" type="list") {
	constant(value="'$TRAVERSAL_ROOT'/base/")
	property(name="hostname")
	constant(value="/")
	property(name="app-name")
	constant(value=".log")
}
template(name="outfmt" type="string" string="%msg%\n")

$rulesetparser rsyslog.rfc5424
local4.debug action(type="omfile" dynafile="dynfile" template="outfmt")
'

startup
{
	printf '<167>1 2003-03-01T01:00:00.000Z ../escape %s - - - traversal-list-template\n' \
		"$TRAVERSAL_APP"
	printf '<167>1 2003-03-01T01:00:00.000Z goodhost %s - - - valid-after-blocked-traversal\n' \
		"$VALID_APP"
} > "${RSYSLOG_DYNNAME}.input"
tcpflood -B -I "${RSYSLOG_DYNNAME}.input"
shutdown_when_empty
wait_shutdown

if [ -f "$TRAVERSAL_OUT" ]; then
	echo "FAIL: list dynafile traversal created output outside template base: $TRAVERSAL_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

if [ ! -f "$VALID_OUT" ]; then
	echo "FAIL: valid list dynafile message did not create: $VALID_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

if [ "$(grep -Fc 'valid-after-blocked-traversal' "$VALID_OUT")" -ne 1 ]; then
	echo "FAIL: valid list dynafile record was not written exactly once"
	cat "$VALID_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

rm -rf "$TRAVERSAL_ROOT"
exit_test
