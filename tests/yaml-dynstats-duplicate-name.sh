#!/bin/bash
# Verify that duplicate YAML dyn_stats bucket names warn through the shared
# dynstats backend while keeping the config valid. The oracle is successful
# -N1 config validation plus the duplicate-name diagnostic; no runtime message
# processing output is involved.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
dyn_stats:
  - name: dup_bucket
    resettable: off
    maxCardinality: 3
  - name: dup_bucket
    resettable: off
    maxCardinality: 9

rulesets:
  - name: main
    filter: "*.*"
    actions:
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
YAMLEOF

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: expected YAML config validation to continue after duplicate dyn_stats bucket warning"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "dynstats: duplicate bucket name 'dup_bucket' in current config set; impstats output may be ambiguous" \
    "${RSYSLOG_DYNNAME}.log"

exit_test
