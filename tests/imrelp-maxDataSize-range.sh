#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify the imrelp maxDataSize framing boundary through RainerScript. The
# oracle is configuration validation: 999,999,999 is accepted, the next value
# and a value above 32-bit SIZE_MAX are rejected, and global(maxMessageSize)
# cannot raise the effective listener limit beyond the same boundary. No
# listener is started or timing involved.
. ${srcdir:=.}/diag.sh init
require_plugin imrelp

write_config() {
	local cfg="$1"
	local global_max="$2"
	local input_max="$3"

	cat >"${cfg}" <<CONF_EOF
global(maxMessageSize="${global_max}")
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="0" maxDataSize="${input_max}")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
}

run_expect_success() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -N1 -f"${cfg}" -M"$RSYSLOG_MODDIR" >"${log}" 2>&1
	local rc=$?
	if [ $rc -ne 0 ]; then
		echo "FAIL: expected ${cfg} to validate"
		cat "${log}"
		error_exit 1
	fi
}

run_expect_failure() {
	local cfg="$1"
	local log="$2"
	local expected="$3"

	../tools/rsyslogd -N1 -f"${cfg}" -M"$RSYSLOG_MODDIR" >"${log}" 2>&1
	local rc=$?
	if [ $rc -eq 0 ]; then
		echo "FAIL: expected ${cfg} to fail validation"
		cat "${log}"
		error_exit 1
	fi
	content_check "${expected}" "${log}"
}

write_config "${RSYSLOG_DYNNAME}.boundary.conf" "999999999" "999999999"
run_expect_success "${RSYSLOG_DYNNAME}.boundary.conf" "${RSYSLOG_DYNNAME}.boundary.log"

write_config "${RSYSLOG_DYNNAME}.input-over.conf" "8192" "1000000000"
run_expect_failure "${RSYSLOG_DYNNAME}.input-over.conf" "${RSYSLOG_DYNNAME}.input-over.log" \
	"maxDataSize (1000000000) exceeds the largest value supported by librelp framing (999999999)"

write_config "${RSYSLOG_DYNNAME}.input-over-32bit.conf" "8192" "4294967296"
run_expect_failure "${RSYSLOG_DYNNAME}.input-over-32bit.conf" "${RSYSLOG_DYNNAME}.input-over-32bit.log" \
	"maxDataSize (4294967296) exceeds the largest value supported by librelp framing (999999999)"

write_config "${RSYSLOG_DYNNAME}.global-over.conf" "1000000000" "999999999"
run_expect_failure "${RSYSLOG_DYNNAME}.global-over.conf" "${RSYSLOG_DYNNAME}.global-over.log" \
	"global parameter maxMessageSize (1000000000) exceeds the largest maxDataSize supported by librelp framing (999999999)"

exit_test
