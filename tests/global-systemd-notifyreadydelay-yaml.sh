#!/bin/bash
# Validate YAML parity for global(systemd.notifyReadyDelay=...). The oracle is
# config validation accepting both binary values through the YAML global
# singleton object, independent of whether the test host runs under systemd.
. ${srcdir:=.}/diag.sh init

modpath="${RSYSLOG_MODDIR}"

run_expect_success() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if [ $rc -ne 0 ]; then
		echo "FAIL: expected ${cfg} to validate"
		cat "${log}"
		error_exit 1
	fi
}

write_yaml_config() {
	local cfg="$1"
	local value="$2"

	cat >"${cfg}" <<YAML_EOF
version: 2
global:
  systemd.notifyReadyDelay: "${value}"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
}

write_yaml_config "${RSYSLOG_DYNNAME}.off.yaml" "off"
run_expect_success "${RSYSLOG_DYNNAME}.off.yaml" "${RSYSLOG_DYNNAME}.off.log"

write_yaml_config "${RSYSLOG_DYNNAME}.on.yaml" "on"
run_expect_success "${RSYSLOG_DYNNAME}.on.yaml" "${RSYSLOG_DYNNAME}.on.log"

exit_test
