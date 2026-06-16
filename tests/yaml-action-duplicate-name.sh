#!/bin/bash
# Verify that duplicate YAML action names warn through the shared action
# backend. The oracle is successful -N1 config validation plus duplicate-name
# diagnostic; no runtime message-processing output is involved.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
templates:
  - name: outfmt
    type: string
    string: "%msg%\n"

rulesets:
  - name: main
    filter: "*.*"
    actions:
      - name: dup_action
        type: omfile
        file: "${RSYSLOG_OUT_LOG}.1"
        template: outfmt
      - name: dup_action
        type: omfile
        file: "${RSYSLOG_OUT_LOG}.2"
        template: outfmt
YAMLEOF
sed -i \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: expected YAML config validation to continue after duplicate action-name warning"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

content_check "action: duplicate name 'dup_action' in current config set; impstats counters may be ambiguous" \
    "${RSYSLOG_DYNNAME}.log"

exit_test
