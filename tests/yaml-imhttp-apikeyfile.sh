#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
IMHTTP_PORT="$(get_free_port)"
generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../contrib/imhttp/.libs/imhttp"'
add_yaml_conf '    ports: "'$IMHTTP_PORT'"'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%\n"'
add_yaml_conf 'inputs:'
add_yaml_imdiag_input
add_yaml_conf '  - type: imhttp'
add_yaml_conf '    endpoint: "/postrequest"'
add_yaml_conf '    ruleset: "main"'
add_yaml_conf '    apiKeyFile: "'$srcdir'/testsuites/imhttp-apikeys.txt"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")'
startup

ret=$(curl -s -o /dev/null -w '%{http_code}' \
  -H 'Authorization: ApiKey secret-token-1' \
  -H Content-Type:application/json \
  http://localhost:$IMHTTP_PORT/postrequest \
  -d '[{"yaml":"apikey"}]')
if [ "$ret" != "200" ]; then
  echo "ERROR: expected 200 for valid API key in YAML config, got $ret"
  error_exit 1
fi

wait_queueempty
shutdown_when_empty
wait_shutdown
content_check '[{"yaml":"apikey"}]'
exit_test
