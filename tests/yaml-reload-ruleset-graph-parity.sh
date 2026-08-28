#!/bin/bash
# Verify that RainerScript and native YAML produce the same normalized runtime
# fingerprint for an equivalent optimized ruleset program.  The same configs
# also contain a second ruleset with one changed expression; its unequal
# fingerprint is the negative oracle that proves the producer observes
# semantic plan changes rather than returning a constant.  imdiag queries the
# already active graph, so no timing assumption or log scraping is involved.
. ${srcdir:=.}/diag.sh init
require_yaml_support

query_ruleset_fingerprint() {
	local response
	response="$(echo "GetReloadRulesetFingerprint $1" |
		"$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")" || error_exit $?
	printf '%s\n' "${response#*: }"
}

generate_conf
add_conf '
ruleset(name="route") {
    set $.copy = $msg;
    if $.copy contains "needle" then stop
}
ruleset(name="changed") {
    set $.copy = $msg;
    if $.copy contains "different" then stop
}
ruleset(name="sink") {
    action(type="omfile" name="sink_action" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
rainer_route="$(query_ruleset_fingerprint route)"
rainer_changed="$(query_ruleset_fingerprint changed)"
rainer_sink="$(query_ruleset_fingerprint sink)"
shutdown_when_empty
wait_shutdown

if [ "$rainer_route" = "$rainer_changed" ]; then
	echo "FAIL: changed RainerScript expression produced the same ruleset fingerprint"
	error_exit 1
fi

generate_conf --yaml-only
cat >>"${TESTCONF_NM}.yaml" <<'YAMLEOF'

rulesets:
  - name: route
    statements:
      - set:
          var: "$.copy"
          expr: "$msg"
      - if: '$.copy contains "needle"'
        then:
          - stop: true
  - name: changed
    statements:
      - set:
          var: "$.copy"
          expr: "$msg"
      - if: '$.copy contains "different"'
        then:
          - stop: true
  - name: sink
    actions:
      - type: omfile
        name: sink_action
        file: "${RSYSLOG_OUT_LOG}"
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${TESTCONF_NM}.yaml"
startup
yaml_route="$(query_ruleset_fingerprint route)"
yaml_changed="$(query_ruleset_fingerprint changed)"
yaml_sink="$(query_ruleset_fingerprint sink)"
shutdown_when_empty
wait_shutdown

if [ "$yaml_route" != "$rainer_route" ]; then
	echo "FAIL: equivalent RainerScript and YAML rulesets produced different fingerprints"
	printf 'RainerScript: %s\nYAML:         %s\n' "$rainer_route" "$yaml_route"
	error_exit 1
fi
if [ "$yaml_route" = "$yaml_changed" ]; then
	echo "FAIL: changed YAML expression produced the same ruleset fingerprint"
	error_exit 1
fi
if [ "$yaml_sink" != "$rainer_sink" ]; then
	echo "FAIL: equivalent named action slots produced different ruleset fingerprints"
	printf 'RainerScript: %s\nYAML:         %s\n' "$rainer_sink" "$yaml_sink"
	error_exit 1
fi

exit_test
