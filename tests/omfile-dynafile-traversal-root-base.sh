#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Ensure a dynafile template rooted at / retains / as its containment base.
# The template's first directory is message-derived; a unique /tmp output
# proves the root base accepts absolute children rather than using the fallback
# guard, which rejects every absolute rendered path.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

export ROOT_APP="${RSYSLOG_DYNNAME}-omfile-root-base"
export ROOT_OUT="/tmp/${ROOT_APP}.log"
rm -f "$ROOT_OUT"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="dynfile" type="string" string="/%HOSTNAME%/%APP-NAME%.log")
template(name="outfmt" type="string" string="%msg%\n")

$rulesetparser rsyslog.rfc5424
local4.debug action(type="omfile" dynafile="dynfile" template="outfmt")
'

startup
printf '<167>1 2003-03-01T01:00:00.000Z tmp %s - - - root-base-output\n' "$ROOT_APP" \
	> "${RSYSLOG_DYNNAME}.input"
tcpflood -B -I "${RSYSLOG_DYNNAME}.input"
shutdown_when_empty
wait_shutdown

if [ ! -f "$ROOT_OUT" ]; then
	echo "FAIL: dynafile template rooted at / did not create: $ROOT_OUT"
	error_exit 1
fi

if ! grep -Fqx 'root-base-output' "$ROOT_OUT"; then
	echo "FAIL: dynafile template rooted at / wrote unexpected content"
	cat "$ROOT_OUT"
	rm -f "$ROOT_OUT"
	error_exit 1
fi

rm -f "$ROOT_OUT"
exit_test
