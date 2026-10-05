#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify YAML parity for imudp BatchSize validation. Configuration validation
# is the oracle: a normal tuning value is accepted while zero and a value that
# cannot be represented by the recvmmsg count are rejected before activation.
. "${srcdir:=.}/diag.sh" init
require_plugin imudp
require_yaml_support

if ! grep -q '^#define HAVE_RECVMMSG 1' ../config.h; then
	echo "imudp BatchSize is intentionally ignored without recvmmsg()"
	exit 77
fi

write_config() {
	local cfg="$1"
	local batch_size="$2"

	cat >"${cfg}" <<YAML_EOF
version: 2
modules:
  - load: "../plugins/imudp/.libs/imudp"
    BatchSize: "${batch_size}"
inputs:
  - type: imudp
    port: "0"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

write_config "${RSYSLOG_DYNNAME}.valid.yaml" "128"
../tools/rsyslogd -C -N1 -f"${RSYSLOG_DYNNAME}.valid.yaml" -M"$RSYSLOG_MODDIR" \
	>"${RSYSLOG_DYNNAME}.valid.log" 2>&1
if [ $? -ne 0 ]; then
	echo "FAIL: expected YAML BatchSize=128 to validate"
	cat "${RSYSLOG_DYNNAME}.valid.log"
	error_exit 1
fi

for batch_size in 0 2147483648; do
	write_config "${RSYSLOG_DYNNAME}.${batch_size}.yaml" "${batch_size}"
	../tools/rsyslogd -C -N1 -f"${RSYSLOG_DYNNAME}.${batch_size}.yaml" -M"$RSYSLOG_MODDIR" \
		>"${RSYSLOG_DYNNAME}.${batch_size}.log" 2>&1
	if [ $? -eq 0 ]; then
		echo "FAIL: expected YAML BatchSize=${batch_size} to fail validation"
		cat "${RSYSLOG_DYNNAME}.${batch_size}.log"
		error_exit 1
	fi
	content_check "valid range is 1..2147483647" "${RSYSLOG_DYNNAME}.${batch_size}.log"
done

exit_test
