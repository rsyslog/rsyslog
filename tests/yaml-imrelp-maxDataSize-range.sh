#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify YAML parity for the imrelp maxDataSize framing boundary. The oracle is
# configuration validation accepting 999,999,999 and rejecting 1,000,000,000
# as well as a value above 32-bit SIZE_MAX; no listener is started and no
# socket-readiness timing is involved.
. ${srcdir:=.}/diag.sh init
require_plugin imrelp
require_yaml_support

write_config() {
	local cfg="$1"
	local input_max="$2"

	cat >"${cfg}" <<YAML_EOF
version: 2
modules:
  - load: "../plugins/imrelp/.libs/imrelp"
inputs:
  - type: imrelp
    port: "0"
    maxDataSize: "${input_max}"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

run_expect_success() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -C -N1 -f"${cfg}" -M"$RSYSLOG_MODDIR" >"${log}" 2>&1
	local rc=$?
	if [ $rc -ne 0 ]; then
		echo "FAIL: expected ${cfg} to validate"
		cat "${log}"
		error_exit 1
	fi
}

write_config "${RSYSLOG_DYNNAME}.boundary.yaml" "999999999"
run_expect_success "${RSYSLOG_DYNNAME}.boundary.yaml" "${RSYSLOG_DYNNAME}.boundary.log"

write_config "${RSYSLOG_DYNNAME}.over.yaml" "1000000000"
../tools/rsyslogd -C -N1 -f"${RSYSLOG_DYNNAME}.over.yaml" -M"$RSYSLOG_MODDIR" \
	>"${RSYSLOG_DYNNAME}.over.log" 2>&1
if [ $? -eq 0 ]; then
	echo "FAIL: expected YAML maxDataSize=1000000000 to fail validation"
	cat "${RSYSLOG_DYNNAME}.over.log"
	error_exit 1
fi
content_check \
	"maxDataSize (1000000000) exceeds the largest value supported by librelp framing (999999999)" \
	"${RSYSLOG_DYNNAME}.over.log"

write_config "${RSYSLOG_DYNNAME}.over-32bit.yaml" "4294967296"
../tools/rsyslogd -C -N1 -f"${RSYSLOG_DYNNAME}.over-32bit.yaml" -M"$RSYSLOG_MODDIR" \
	>"${RSYSLOG_DYNNAME}.over-32bit.log" 2>&1
if [ $? -eq 0 ]; then
	echo "FAIL: expected YAML maxDataSize=4294967296 to fail validation"
	cat "${RSYSLOG_DYNNAME}.over-32bit.log"
	error_exit 1
fi
content_check \
	"maxDataSize (4294967296) exceeds the largest value supported by librelp framing (999999999)" \
	"${RSYSLOG_DYNNAME}.over-32bit.log"

exit_test
