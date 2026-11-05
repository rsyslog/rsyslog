#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify malformed and out-of-range CIDR exclusions fail during config
# validation. The oracle is a normal rsyslogd -N1 failure containing the
# mmanon-specific diagnostic; a signal-like exit would indicate a parser or
# cleanup defect rather than successful validation.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmanon/.libs/mmanon")
action(type="mmanon" excludeSubnets=["192.0.2.0/33"])
'

rsyslogd_config_check >"${RSYSLOG_DYNNAME}.config-check.log" 2>&1
status=$?
if [ "$status" -eq 0 ]; then
	echo "Expected configuration failure for an out-of-range mmanon CIDR prefix"
	error_exit 1
fi
if [ "$status" -gt 128 ]; then
	echo "rsyslogd config check exited like it received a signal: $status"
	cat "${RSYSLOG_DYNNAME}.config-check.log"
	error_exit 1
fi

custom_content_check "excludeSubnets entry '192.0.2.0/33' is not a valid IPv4 or IPv6 CIDR" \
	"${RSYSLOG_DYNNAME}.config-check.log"
exit_test
