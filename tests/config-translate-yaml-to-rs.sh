#!/bin/bash
# check canonical translation from YAML to RainerScript.
#
# Part of the testbench for rsyslog.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
modpath="../runtime/.libs:../plugins/omstdout/.libs:../.libs"

cat > "${RSYSLOG_DYNNAME}.yaml" <<'YAML_EOF'
version: 2
modules:
  - load: "omstdout"
mainqueue:
  queue.type: "Direct"
rulesets:
  - name: "main"
    script: |
      action(type="omstdout")
YAML_EOF

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.yaml" -F rainerscript -o "${RSYSLOG_DYNNAME}.out.conf" -M"$modpath"

cat > "${RSYSLOG_DYNNAME}.expected.conf" <<'RS_EOF'
main_queue(queue.type="Direct")

module(load="omstdout")

ruleset(name="main") {
  action(type="omstdout")
}

RS_EOF
cmp_exact_file "${RSYSLOG_DYNNAME}.expected.conf" "${RSYSLOG_DYNNAME}.out.conf"

../tools/rsyslogd -N1 -f "${RSYSLOG_DYNNAME}.out.conf" -M"$modpath"

echo SUCCESS: YAML to RainerScript translation
exit_test
