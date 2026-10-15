#!/bin/bash
# Validate native YAML parsing parity for reloadOnHUP. The config-check oracle
# accepts off, validate, and on and rejects the invalid value with the same
# diagnostic as RainerScript. Native YAML live-HUP coverage is kept in the
# imtcp-backed mode tests so no RainerScript include is involved.
. ${srcdir:=.}/diag.sh init
require_yaml_support

modpath="${RSYSLOG_MODDIR}"

run_expect_status() {
	local expected="$1"
	local cfg="$2"
	local log="$3"

	../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if [ "${rc}" -ne "${expected}" ]; then
		echo "FAIL: expected ${cfg} to exit ${expected}, got ${rc}"
		cat "${log}"
		error_exit 1
	fi
}

for mode in off validate on invalid; do
	cfg="${RSYSLOG_DYNNAME}.${mode}.yaml"
	cat >"${cfg}" <<YAML_EOF
version: 2
global:
  config.reloadOnHUP: "${mode}"
rulesets:
  - name: main
    actions:
      - type: omfile
        file: "${RSYSLOG_DYNNAME}.out"
YAML_EOF
	if [ "${mode}" = invalid ]; then
		run_expect_status 1 "${cfg}" "${RSYSLOG_DYNNAME}.${mode}.log"
	else
		run_expect_status 0 "${cfg}" "${RSYSLOG_DYNNAME}.${mode}.log"
	fi
done
content_check "invalid config.reloadOnHUP value 'invalid'; expected off, validate, or on" \
	"${RSYSLOG_DYNNAME}.invalid.log"

exit_test
