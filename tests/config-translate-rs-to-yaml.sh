#!/bin/bash
# check canonical translation from RainerScript to YAML.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
modpath="../runtime/.libs:../plugins/omstdout/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.conf" <<'RS_EOF'
module(load="omstdout")
main_queue(queue.type="Direct")
ruleset(name="main") {
  action(type="omstdout")
}
RS_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.conf" -F yaml -o "${RSYSLOG_DYNNAME}.yaml" -M"$modpath"

cat > "${RSYSLOG_DYNNAME}.expected.yaml" <<'YAML_EOF'
version: 2

mainqueue:
  queue.type: "Direct"

modules:
  - load: "omstdout"
rulesets:
  - name: "main"
    script: |
        action(type="omstdout")
YAML_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.yaml" "${RSYSLOG_DYNNAME}.yaml"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -M"$modpath"

echo SUCCESS: RainerScript to YAML translation
exit_test
