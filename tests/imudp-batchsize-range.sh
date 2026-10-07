#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify RainerScript imudp BatchSize validation before the value is narrowed
# to the recvmmsg API's integer count. Configuration validation is the oracle:
# ordinary positive sizes are accepted and zero, negative, and above-INT_MAX
# values are rejected without starting a listener or allocating receive buffers.
. "${srcdir:=.}/diag.sh" init
require_plugin imudp

if ! grep -q '^#define HAVE_RECVMMSG 1' ../config.h; then
	echo "imudp BatchSize is intentionally ignored without recvmmsg()"
	exit 77
fi

write_config() {
	local cfg="$1"
	local batch_size="$2"

	cat >"${cfg}" <<CONF_EOF
module(load="../plugins/imudp/.libs/imudp" BatchSize="${batch_size}")
input(type="imudp" port="0")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
}

run_expect_success() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -N1 -f"${cfg}" -M"$RSYSLOG_MODDIR" >"${log}" 2>&1
	if [ $? -ne 0 ]; then
		echo "FAIL: expected BatchSize in ${cfg} to validate"
		cat "${log}"
		error_exit 1
	fi
}

run_expect_failure() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -N1 -f"${cfg}" -M"$RSYSLOG_MODDIR" >"${log}" 2>&1
	if [ $? -eq 0 ]; then
		echo "FAIL: expected BatchSize in ${cfg} to fail validation"
		cat "${log}"
		error_exit 1
	fi
	content_check "valid range is 1..2147483647" "${log}"
}

for batch_size in 1 32 128; do
	write_config "${RSYSLOG_DYNNAME}.${batch_size}.conf" "${batch_size}"
	run_expect_success "${RSYSLOG_DYNNAME}.${batch_size}.conf" "${RSYSLOG_DYNNAME}.${batch_size}.log"
done

for batch_size in 0 -1 2147483648; do
	write_config "${RSYSLOG_DYNNAME}.${batch_size}.conf" "${batch_size}"
	run_expect_failure "${RSYSLOG_DYNNAME}.${batch_size}.conf" "${RSYSLOG_DYNNAME}.${batch_size}.log"
done

exit_test
