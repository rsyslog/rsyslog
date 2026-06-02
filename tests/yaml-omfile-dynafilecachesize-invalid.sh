#!/bin/bash
# Ensure YAML omfile actions normalize invalid dynaFileCacheSize values during
# startup while still allowing dynafile writes.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: dynfile
    type: string
    string: "${RSYSLOG_DYNNAME}.out.log"
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"

rulesets:
  - name: main
    filter: ':msg, contains, "msg:"'
    actions:
      - type: omfile
        dynafile: dynfile
        template: outfmt
        dynaFileCacheSize: 0
YAMLEOF
sed -i "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag msg:yaml-normalized-cache-size\""
shutdown_when_empty
wait_shutdown

grep "DynaFileCacheSize must be greater 0 (0 given), changed to 1." "${RSYSLOG_DYNNAME}.started" > /dev/null || {
    echo "FAIL: expected normalization message not found in YAML startup log"
    cat "${RSYSLOG_DYNNAME}.started"
    error_exit 1
}

printf 'yaml-normalized-cache-size\n' > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.out.log" || {
    echo "FAIL: YAML dynafile write did not succeed after normalization"
    cat "${RSYSLOG_DYNNAME}.out.log"
    error_exit 1
}

exit_test
