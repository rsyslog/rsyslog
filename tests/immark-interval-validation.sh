#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject nonpositive mark intervals during configuration validation. Run only
# -N1: no input thread or negative-interval runtime loop is ever started.
# Default and positive values must validate; zero must retain its rejection.
# Check those controls before negative values so a module/config setup failure
# cannot masquerade as the regression. Require the module-specific diagnostic
# as well as rejection. The YAML wrapper tests the same module parameter;
# RainerScript also covers the legacy directive. No timing oracle is needed.
. ${srcdir:=.}/diag.sh init
require_plugin immark

validate_interval() {
    local mode=$1 value=$2 expected=$3 status
    generate_conf
    case "$mode" in
    yaml)
        printf 'modules:\n  - load: ../plugins/immark/.libs/immark\n' > "${RSYSLOG_DYNNAME}.yaml"
        if [ "$value" != default ]; then
            printf '    interval: %s\n' "$value" >> "${RSYSLOG_DYNNAME}.yaml"
        fi
        add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")'
        ;;
    legacy)
        add_conf '$ModLoad ../plugins/immark/.libs/immark
$MarkMessagePeriod '$value
        ;;
    rs)
        if [ "$value" = default ]; then
            add_conf 'module(load="../plugins/immark/.libs/immark")'
        else
            add_conf 'module(load="../plugins/immark/.libs/immark" interval="'$value'")'
        fi
        ;;
    esac
    cp "${TESTCONF_NM}.conf" "${RSYSLOG_DYNNAME}.${mode}.${value}.conf" || error_exit 1
    ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
        > "${RSYSLOG_DYNNAME}.${mode}.${value}.log" 2>&1
    status=$?
    cat "${RSYSLOG_DYNNAME}.${mode}.${value}.log"
    if [ "$status" -ne "$expected" ]; then
        error_exit 1 "$mode interval=$value: expected validation status $expected, got $status"
    fi
    if [ "$expected" -ne 0 ]; then
        grep -F 'immark: mark message period' "${RSYSLOG_DYNNAME}.${mode}.${value}.log" >/dev/null ||
            error_exit 1 "missing immark interval validation diagnostic"
    fi
    echo "$mode interval=$value: PASS"
}

if [ "${IMMARK_INTERVAL_YAML:-}" = YES ]; then
    modes=yaml
else
    modes='rs legacy'
fi
for mode in $modes; do
    if [ "$mode" != legacy ]; then validate_interval "$mode" default 0; fi
    validate_interval "$mode" 1 0
    validate_interval "$mode" 0 1
done
for mode in $modes; do
    validate_interval "$mode" -1 1
    validate_interval "$mode" -1200 1
done
exit_test
