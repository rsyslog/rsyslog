#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Reproduce dynafile path traversal through a message-derived hostname.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

export TRAVERSAL_APP="${RSYSLOG_DYNNAME}-omfile-traversal"
export TRAVERSAL_ROOT="${RSYSLOG_DYNNAME}.jail"
export TRAVERSAL_OUT="${TRAVERSAL_ROOT}/escape/${TRAVERSAL_APP}.log"
rm -rf "$TRAVERSAL_ROOT"
mkdir -p "${TRAVERSAL_ROOT}/base" "${TRAVERSAL_ROOT}/escape"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="dynfile" type="string" string="'$TRAVERSAL_ROOT'/base/%HOSTNAME%/%APP-NAME%.log")
template(name="outfmt" type="string" string="%msg%\n")

$rulesetparser rsyslog.rfc5424
local4.debug action(type="omfile" dynafile="dynfile" template="outfmt")
'

startup
printf '<167>1 2003-03-01T01:00:00.000Z ../escape %s - - - traversal-repro\n' "$TRAVERSAL_APP" \
	> "${RSYSLOG_DYNNAME}.input"
tcpflood -B -I "${RSYSLOG_DYNNAME}.input"
shutdown_when_empty
wait_shutdown

if [ -f "$TRAVERSAL_OUT" ]; then
	echo "FAIL: dynafile traversal created output outside template base: $TRAVERSAL_OUT"
	rm -rf "$TRAVERSAL_ROOT"
	error_exit 1
fi

rm -rf "$TRAVERSAL_ROOT"
exit_test
