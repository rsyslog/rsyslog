#!/bin/bash
# Tests the YAML-native statements: unset: and call_indirect: constructs.
# rs1 sets a variable to a ruleset name, then uses call_indirect to dispatch
# to that ruleset.  The target ruleset (rs2) uses unset: to clear a variable
# and then writes matching messages to the output.
#
# Verification: after unset:, %.scratch% is written to a separate file via
# the verifyfmt template.  The test fails if the pre-unset marker value
# appears there, proving that unset: actually cleared the variable.
#
# Added 2025, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=200
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: string
    string: "%msg:F,58:2%\n"
  - name: verifyfmt
    type: string
    string: "%.scratch%\n"

rulesets:
  - name: rs2
    statements:
      - unset: "$.scratch"
      - type: omfile
        template: verifyfmt
        file: "${RSYSLOG_DYNNAME}.scratch"
      - if: '$msg contains "msgnum:"'
        action:
          type: omfile
          template: outfmt
          file: "${RSYSLOG_OUT_LOG}"
      - stop: true

  - name: rs1
    statements:
      - set:
          var: "$.scratch"
          expr: '"will be unset"'
      - set:
          var: "$.target"
          expr: '"rs2"'
      - call_indirect: "$.target"
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"
sed -i "s|\${RSYSLOG_DYNNAME}|${RSYSLOG_DYNNAME}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="rs1")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
# Verify $.scratch was actually cleared by unset: — the pre-unset marker
# must not appear in the verify file (each line should be empty after unset).
# Explicitly fail when the file is absent so a missing write cannot produce
# a false pass (grep exits 2 on read error, which would look like "no match").
if [ ! -f "${RSYSLOG_DYNNAME}.scratch" ]; then
    echo "FAIL: verify file '${RSYSLOG_DYNNAME}.scratch' was not created (unset write never ran)"
    exit 1
fi
if grep -q "will be unset" "${RSYSLOG_DYNNAME}.scratch"; then
    echo "FAIL: unset: did not clear \$.scratch (marker 'will be unset' still present)"
    head -3 "${RSYSLOG_DYNNAME}.scratch"
    exit 1
fi
exit_test
