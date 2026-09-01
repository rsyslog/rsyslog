#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# A json-typed parser captures its value as a well-formed JSON span and marks
# it LN_FFIELD_RAW_JSON. The turbo materializer must reparse that span into a
# nested object; storing it as an opaque string turns {"kc":{...}} into
# {"kc":"{...}"} and every $!kc!field lookup below it resolves to nothing.
#
# Oracle: the nested leaf is readable through the subscript path.
#
# The flag is only exposed by a liblognorm carrying the turbo parser-parity
# change. Against an older one the reparse branch is compiled out by the compat
# fallback in mmnormalize.c, so there is nothing to assert and the test skips.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

lognorm_incdir=$(pkg-config --variable=includedir lognorm 2>/dev/null)
if ! grep -qs 'LN_FFIELD_RAW_JSON' "${lognorm_incdir:-/usr/include}/lognorm-turbo.h"; then
	printf 'SKIP: liblognorm does not expose LN_FFIELD_RAW_JSON, so the turbo\n'
	printf 'SKIP: json-field reparse is compiled out and cannot be exercised.\n'
	skip_test
fi

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="norm")

# reading a leaf below kc only resolves when kc is a real object
template(name="leaf" type="string" string="%$!kc!inner!num%\n")

ruleset(name="norm") {
	action(type="mmnormalize" useRawMsg="on" turbo="on"
		rule="rule=:%kc:json%")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="leaf")
}
'
startup
tcpflood -m1 -M "\"{\\\"inner\\\":{\\\"num\\\":0}}\""
shutdown_when_empty
wait_shutdown
export EXPECTED='0'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
