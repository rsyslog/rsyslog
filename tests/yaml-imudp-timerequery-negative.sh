#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# YAML companion of imudp-timerequery-negative.sh: a negative imudp
# timeRequery module parameter must be rejected at config load, and zero
# must be accepted. Oracle: rsyslogd -N1 fails with the non-negative
# parameter error for -1 and succeeds for 0.
. "${srcdir:=.}/diag.sh" init
require_yaml_support
require_plugin imudp
modpath="../runtime/.libs:../.libs"

run_yaml() {
    local name="$1"
    local value="$2"
    local cfg="${RSYSLOG_DYNNAME}.${name}.yaml"
    local log="${RSYSLOG_DYNNAME}.${name}.log"

    cat > "$cfg" <<YAML_EOF
version: 2
modules:
  - load: "../plugins/imudp/.libs/imudp"
    timeRequery: "$value"
inputs:
  - type: imudp
    address: "127.0.0.1"
    port: "0"
    ruleset: out
rulesets:
  - name: out
    script: |
      action(type="omfile" file="${RSYSLOG_OUT_LOG}")
YAML_EOF

    ../tools/rsyslogd -N1 -f "$cfg" -M"$modpath" > "$log" 2>&1
}

run_yaml negative -1
if [ $? -eq 0 ]; then
    echo "FAIL: YAML config check unexpectedly succeeded for timeRequery=-1"
    cat "${RSYSLOG_DYNNAME}.negative.log"
    error_exit 1
fi
content_check "parameter 'timerequery' cannot be less than zero (was -1)" "${RSYSLOG_DYNNAME}.negative.log"

run_yaml zero 0
if [ $? -ne 0 ]; then
    echo "FAIL: YAML timeRequery=0 must be accepted"
    cat "${RSYSLOG_DYNNAME}.zero.log"
    error_exit 1
fi

exit_test
