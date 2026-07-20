#!/bin/bash
# added 2026-07-20 by Codex, released under ASL 2.0
# Provides YAML parity for the aggregate zstd window limit and secure-default
# policy. Configuration-validation status and diagnostics are the oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin lmzstdw ../runtime

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

run_expect_failure() {
    local cfg="$1"
    local log="$2"
    ../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
    local rc=$?
    if [ $rc -eq 0 ]; then
        echo "FAIL: expected ${cfg} to fail validation"
        cat "${log}"
        error_exit 1
    fi
}

write_input_config() {
    local cfg="$1"
    local policy="$2"
    local mode="$3"
    local driver="$4"
    local limit="$5"
    cat >"${cfg}" <<YAML_EOF
version: 2
global:
  compatibility.defaults.secure: "${policy}"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    port: "0"
    ruleset: sink
    compression.mode: "${mode}"
    compression.driver: "${driver}"
${limit}
rulesets:
  - name: sink
    statements:
      - type: omfile
        file: "/dev/null"
YAML_EOF
}

warning='compression.maxTotalZstdWindowBytes was not explicitly set'
strict_error='compatibility.defaults.secure="strict" requires an explicit positive compression.maxTotalZstdWindowBytes'

write_input_config "${RSYSLOG_DYNNAME}.yaml-warn-omitted.yaml" warn stream:always zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.yaml-warn-omitted.yaml" "${RSYSLOG_DYNNAME}.yaml-warn-omitted.log"
content_check "${warning}" "${RSYSLOG_DYNNAME}.yaml-warn-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-backcompat-omitted.yaml" backward-compatible stream:always zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.yaml-backcompat-omitted.yaml" \
    "${RSYSLOG_DYNNAME}.yaml-backcompat-omitted.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.yaml-backcompat-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-warn-zero.yaml" warn stream:always zstd \
    '    compression.maxTotalZstdWindowBytes: "0"'
run_expect_success "${RSYSLOG_DYNNAME}.yaml-warn-zero.yaml" "${RSYSLOG_DYNNAME}.yaml-warn-zero.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.yaml-warn-zero.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-warn-positive.yaml" warn stream:always zstd \
    '    compression.maxTotalZstdWindowBytes: "2097152"'
run_expect_success "${RSYSLOG_DYNNAME}.yaml-warn-positive.yaml" "${RSYSLOG_DYNNAME}.yaml-warn-positive.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.yaml-warn-positive.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-strict-omitted.yaml" strict stream:always zstd ""
run_expect_failure "${RSYSLOG_DYNNAME}.yaml-strict-omitted.yaml" "${RSYSLOG_DYNNAME}.yaml-strict-omitted.log"
content_check "${strict_error}" "${RSYSLOG_DYNNAME}.yaml-strict-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-strict-zero.yaml" strict stream:always zstd \
    '    compression.maxTotalZstdWindowBytes: "0"'
run_expect_failure "${RSYSLOG_DYNNAME}.yaml-strict-zero.yaml" "${RSYSLOG_DYNNAME}.yaml-strict-zero.log"
content_check "${strict_error}" "${RSYSLOG_DYNNAME}.yaml-strict-zero.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-strict-positive.yaml" strict stream:always zstd \
    '    compression.maxTotalZstdWindowBytes: "2097152"'
run_expect_success "${RSYSLOG_DYNNAME}.yaml-strict-positive.yaml" "${RSYSLOG_DYNNAME}.yaml-strict-positive.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-warn-zlib.yaml" warn stream:always zlib ""
run_expect_success "${RSYSLOG_DYNNAME}.yaml-warn-zlib.yaml" "${RSYSLOG_DYNNAME}.yaml-warn-zlib.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.yaml-warn-zlib.log"

write_input_config "${RSYSLOG_DYNNAME}.yaml-warn-inactive.yaml" warn none zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.yaml-warn-inactive.yaml" "${RSYSLOG_DYNNAME}.yaml-warn-inactive.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.yaml-warn-inactive.log"

cat >"${RSYSLOG_DYNNAME}.yaml-strict-module-inherit.yaml" <<'YAML_EOF'
version: 2
global:
  compatibility.defaults.secure: "strict"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
    compression.mode: "stream:always"
    compression.driver: "zstd"
    compression.maxTotalZstdWindowBytes: "2097152"
inputs:
  - type: imtcp
    port: "0"
    ruleset: sink
rulesets:
  - name: sink
    statements:
      - type: omfile
        file: "/dev/null"
YAML_EOF
run_expect_success "${RSYSLOG_DYNNAME}.yaml-strict-module-inherit.yaml" \
    "${RSYSLOG_DYNNAME}.yaml-strict-module-inherit.log"

exit_test
