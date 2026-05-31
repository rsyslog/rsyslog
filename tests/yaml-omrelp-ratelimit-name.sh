#!/bin/bash
# Ensure YAML omrelp actions accept ratelimit.name.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
ratelimits:
  - name: yaml_omrelp_limit
    interval: 2
    burst: 5

modules:
  - load: "../plugins/omrelp/.libs/omrelp"

rulesets:
  - name: main
    actions:
      - type: omrelp
        target: "127.0.0.1"
        port: "2514"
        ratelimit.name: yaml_omrelp_limit
YAMLEOF

startup
shutdown_when_empty
wait_shutdown
exit_test
