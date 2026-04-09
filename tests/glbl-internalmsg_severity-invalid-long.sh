#!/bin/bash
# Regression test for long and overflowing severity names reaching decodeSyslogName().
. ${srcdir:=.}/diag.sh init

run_bad_severity_check() {
    instance="$1"
    severity_value="$2"
    log_file="$3"

    generate_conf "$instance"
    add_conf "
global(internalmsg.severity=\"$severity_value\")
action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\")
    " "$instance"

    ../tools/rsyslogd -C -N1 \
        -M"${RSYSLOG_MODDIR}" \
        -f"${TESTCONF_NM}${instance}.conf" \
        > "${log_file}" 2>&1
    rc=$?
    if [ $rc -eq 0 ]; then
        echo "FAIL: rsyslogd should reject invalid internalmsg.severity=${severity_value}"
        error_exit 1
    fi

    content_check 'invalid internalmsg.severity value' "${log_file}"
}

long_symbolic="$(printf 'A%.0s' $(seq 1 160))"
run_bad_severity_check 1 "${long_symbolic}" "${RSYSLOG_DYNNAME}.long.error.log"
run_bad_severity_check 2 "999999999999999999999999" "${RSYSLOG_DYNNAME}.overflow.error.log"

exit_test
