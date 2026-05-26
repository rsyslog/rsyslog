#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# Verify YAML imhttp metrics API-key authentication while the HTTP listener uses
# an OS-assigned port. The HTTP status and metrics response are the oracle.

. ${srcdir:=.}/diag.sh init
require_yaml_support
IMHTTP_PORT_FILE="$RSYSLOG_DYNNAME.imhttp.port"
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../contrib/imhttp/.libs/imhttp"'
add_yaml_conf '    ports: "0"'
add_yaml_conf '    listenPortFileName: "'$IMHTTP_PORT_FILE'"'
add_yaml_conf '    metricsPath: "/metrics"'
add_yaml_conf '    metricsApiKeyFile: "'$srcdir'/testsuites/imhttp-apikeys.txt"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imhttp'
add_yaml_conf '    endpoint: "/unused"'
add_yaml_conf '    ruleset: "main"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'.unused")'
startup
assign_file_content IMHTTP_PORT "$IMHTTP_PORT_FILE"

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
