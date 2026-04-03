#!/bin/bash
# check canonical translation from RainerScript to YAML.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
main_queue(queue.type="Direct")
ruleset(name="main") {
  action(type="omfile" file="/var/log/sample.log")
}
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath"

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<'YAML_EOF'
version: 2

mainqueue:
  queue.type: "Direct"

rulesets:
  - name: "main"
    actions:
      - type: "omfile"
        file: "/var/log/sample.log"
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -M"$modpath"

echo SUCCESS: RainerScript to YAML translation
exit_test
