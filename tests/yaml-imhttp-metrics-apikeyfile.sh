#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
IMHTTP_PORT="$(get_free_port)"
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../contrib/imhttp/.libs/imhttp"'
add_yaml_conf '    ports: "'$IMHTTP_PORT'"'
add_yaml_conf '    metricsPath: "/metrics"'
add_yaml_conf '    metricsApiKeyFile: "'$srcdir'/testsuites/imhttp-apikeys.txt"'
add_yaml_conf 'inputs:'
add_yaml_imdiag_input
add_yaml_conf '  - type: imhttp'
add_yaml_conf '    endpoint: "/unused"'
add_yaml_conf '    ruleset: "main"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'.unused")'
startup

ret=$(curl -s -o /dev/null -w '%{http_code}' http://localhost:$IMHTTP_PORT/metrics)
if [ "$ret" != "401" ]; then
  echo "ERROR: expected 401 without API key in YAML metrics config, got $ret"
  error_exit 1
fi

curl -s -H 'Authorization: ApiKey secret-token-1' \
  http://localhost:$IMHTTP_PORT/metrics > "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
content_check "imhttp_up 1"
exit_test
