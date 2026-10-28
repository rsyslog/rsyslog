#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Mapping codes must contain digits, including after an optional sign. Compare
# sign-only values with established invalid and valid numeric controls. The
# oracle is rsyslogd -N1's process status: invalid action construction must fail
# configuration validation. Startup/configuration logs are retained only for
# failure diagnostics because no usable daemon output exists in this mode.
# YAML uses its ruleset script for the module's existing legacy-only interface;
# this test does not add or assume native YAML/module parameter support.
. ${srcdir:=.}/diag.sh init
require_plugin mmsnmptrapd
if [ "${MMSNMP_SEVERITY_YAML:-0}" = 1 ]; then require_yaml_support; fi
failed=0
run_case() {
    local name="$1" code="$2" expected="$3" status conf
    conf="${RSYSLOG_DYNNAME}.${name}.conf"
    if [ "${MMSNMP_SEVERITY_YAML:-0}" = 1 ]; then
        conf="${RSYSLOG_DYNNAME}.${name}.yaml"
        cat > "$conf" <<YAML_EOF
version: 2
modules:
  - load: "../plugins/mmsnmptrapd/.libs/mmsnmptrapd"
rulesets:
  - name: main
    script: |
      \$mmsnmptrapdSeverityMapping warning/$code
      *.* :mmsnmptrapd:
      action(type="omfile" file="/dev/null")
YAML_EOF
    else
        printf '%s\n' 'module(load="../plugins/mmsnmptrapd/.libs/mmsnmptrapd")'             '$mmsnmptrapdSeverityMapping warning/'"$code"             '*.* :mmsnmptrapd:' 'action(type="omfile" file="/dev/null")' > "$conf"
    fi
    if ../tools/rsyslogd -C -N1 -f "$conf" -M "$RSYSLOG_MODDIR" > "${conf}.log" 2>&1; then
        status=0
    else
        status=$?
    fi
    printf '%s code=%s expected=%s status=%s\n' "$name" "$code" "$expected" "$status"
    if { [ "$expected" = valid ] && [ "$status" -ne 0 ]; } ||        { [ "$expected" = invalid ] && [ "$status" -eq 0 ]; }; then
        cat "${conf}.log"
        failed=1
    fi
}
run_case zero 0 valid
run_case seven 7 valid
run_case plus-four +4 valid
run_case minus-zero -0 valid
run_case bad-letter x invalid
run_case bad-high 8 invalid
run_case bad-negative -1 invalid
run_case sign-plus + invalid
run_case sign-minus - invalid
if [ "$failed" -ne 0 ]; then error_exit 1; fi
exit_test
