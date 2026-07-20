#!/bin/bash
# added 2026-07-20 by Codex, released under ASL 2.0
# Validates the RainerScript secure-default policy for the aggregate zstd
# decoder-window limit. Configuration-validation exit status and startup
# diagnostics are the oracle; no listener runtime timing is involved.
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
    cat >"${cfg}" <<CONF_EOF
global(compatibility.defaults.secure="${policy}")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" compression.mode="${mode}" compression.driver="${driver}" ${limit})
action(type="omfile" file="/dev/null")
CONF_EOF
}

warning='compression.maxTotalZstdWindowBytes was not explicitly set'
strict_error='compatibility.defaults.secure="strict" requires an explicit positive compression.maxTotalZstdWindowBytes'

write_input_config "${RSYSLOG_DYNNAME}.warn-omitted.conf" warn stream:always zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.warn-omitted.conf" "${RSYSLOG_DYNNAME}.warn-omitted.log"
content_check "${warning}" "${RSYSLOG_DYNNAME}.warn-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.backcompat-omitted.conf" backward-compatible stream:always zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.backcompat-omitted.conf" "${RSYSLOG_DYNNAME}.backcompat-omitted.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.backcompat-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.warn-zero.conf" warn stream:always zstd \
    'compression.maxTotalZstdWindowBytes="0"'
run_expect_success "${RSYSLOG_DYNNAME}.warn-zero.conf" "${RSYSLOG_DYNNAME}.warn-zero.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.warn-zero.log"

write_input_config "${RSYSLOG_DYNNAME}.warn-positive.conf" warn stream:always zstd \
    'compression.maxTotalZstdWindowBytes="2097152"'
run_expect_success "${RSYSLOG_DYNNAME}.warn-positive.conf" "${RSYSLOG_DYNNAME}.warn-positive.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.warn-positive.log"

write_input_config "${RSYSLOG_DYNNAME}.strict-omitted.conf" strict stream:always zstd ""
run_expect_failure "${RSYSLOG_DYNNAME}.strict-omitted.conf" "${RSYSLOG_DYNNAME}.strict-omitted.log"
content_check "${strict_error}" "${RSYSLOG_DYNNAME}.strict-omitted.log"

write_input_config "${RSYSLOG_DYNNAME}.strict-zero.conf" strict stream:always zstd \
    'compression.maxTotalZstdWindowBytes="0"'
run_expect_failure "${RSYSLOG_DYNNAME}.strict-zero.conf" "${RSYSLOG_DYNNAME}.strict-zero.log"
content_check "${strict_error}" "${RSYSLOG_DYNNAME}.strict-zero.log"

write_input_config "${RSYSLOG_DYNNAME}.strict-positive.conf" strict stream:always zstd \
    'compression.maxTotalZstdWindowBytes="2097152"'
run_expect_success "${RSYSLOG_DYNNAME}.strict-positive.conf" "${RSYSLOG_DYNNAME}.strict-positive.log"

write_input_config "${RSYSLOG_DYNNAME}.warn-zlib.conf" warn stream:always zlib ""
run_expect_success "${RSYSLOG_DYNNAME}.warn-zlib.conf" "${RSYSLOG_DYNNAME}.warn-zlib.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.warn-zlib.log"

write_input_config "${RSYSLOG_DYNNAME}.warn-inactive.conf" warn none zstd ""
run_expect_success "${RSYSLOG_DYNNAME}.warn-inactive.conf" "${RSYSLOG_DYNNAME}.warn-inactive.log"
check_not_present "${warning}" "${RSYSLOG_DYNNAME}.warn-inactive.log"

cat >"${RSYSLOG_DYNNAME}.strict-module-inherit.conf" <<'CONF_EOF'
global(compatibility.defaults.secure="strict")
module(load="../plugins/imtcp/.libs/imtcp"
       compression.mode="stream:always"
       compression.driver="zstd"
       compression.maxTotalZstdWindowBytes="2097152")
input(type="imtcp" port="0")
action(type="omfile" file="/dev/null")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.strict-module-inherit.conf" \
    "${RSYSLOG_DYNNAME}.strict-module-inherit.log"

exit_test
