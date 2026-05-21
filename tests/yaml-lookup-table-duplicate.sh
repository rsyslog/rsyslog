#!/bin/bash
# Verify that duplicate YAML lookup_tables names are rejected by the shared
# lookup_table backend. The oracle is a failed -N1 config check with the same
# duplicate-name diagnostic as RainerScript, before runtime reloader resources
# can be allocated for the duplicate.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
lookup_tables:
  - name: xlate
    file: "${RSYSLOG_DYNNAME}.xlate.lkp_tbl"
  - name: xlate
    file: "${RSYSLOG_DYNNAME}.xlate.lkp_tbl"

templates:
  - name: outfmt
    type: string
    string: "%msg%\n"

actions:
  - type: omfile
    file: "${RSYSLOG_OUT_LOG}"
    template: outfmt
YAMLEOF
sed -i \
    -e "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" \
    -e "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" \
    "${RSYSLOG_DYNNAME}.yaml"

cp -f "$srcdir/testsuites/xlate.lkp_tbl" "$RSYSLOG_DYNNAME.xlate.lkp_tbl"
../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.log" 2>&1
if [ $? -eq 0 ]; then
    echo "FAIL: expected YAML config validation failure for duplicate lookup_table name"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
fi

grep -F "lookup_table: duplicate name 'xlate' in current config set" "${RSYSLOG_DYNNAME}.log" >/dev/null || {
    echo "FAIL: expected duplicate lookup_table name error message"
    cat "${RSYSLOG_DYNNAME}.log"
    error_exit 1
}

exit_test
