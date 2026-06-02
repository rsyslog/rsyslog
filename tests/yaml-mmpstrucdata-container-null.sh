#!/bin/bash
# Verify YAML configuration parity for mmpstrucdata container and no-SD null
# handling. The YAML frontend must pass the same action parameters to the
# module as RainerScript, and the oracle is the configured omfile output after
# synchronized shutdown.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
$DefaultRuleset main
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/mmpstrucdata/.libs/mmpstrucdata"

templates:
  - name: outfmt
    type: string
    string: "%$!structured-data%\n"

rulesets:
  - name: main
    statements:
      - type: mmpstrucdata
        jsonRoot: "$!structured-data"
        container: custom-yaml-sd
      - if: '$msg contains "MMPSTRUCDATA"'
        then:
          - type: omfile
            file: "${RSYSLOG_OUT_LOG}"
            template: outfmt
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

startup
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid [test@32473 key="value"] MMPSTRUCDATA with sd'
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid - MMPSTRUCDATA without sd'
shutdown_when_empty
wait_shutdown
export EXPECTED='{ "custom-yaml-sd": { "test@32473": { "key": "value" } } }
{ "custom-yaml-sd": null }'
cmp_exact
exit_test
