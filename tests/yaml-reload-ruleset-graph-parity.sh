#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify that RainerScript and native YAML produce the same normalized runtime
# fingerprint for an equivalent optimized ruleset program.  The same configs
# also contain a second ruleset with one changed expression; its unequal
# fingerprint is the negative oracle that proves the producer observes
# semantic plan changes rather than returning a constant.  imdiag queries the
# already active graph, so no timing assumption or log scraping is involved.
# An explicit name matching the generated fallback must retain named identity,
# unlike an anonymous action; both frontends must preserve this distinction.
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
ruleset(name="generatedSink") {
    action(type="omfile" name="action-0-builtin:omfile" file="'$RSYSLOG_OUT_LOG'")
}
ruleset(name="anonymousSink") {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
}
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
rainer_generated="$(query_ruleset_fingerprint generatedSink)"
rainer_anonymous="$(query_ruleset_fingerprint anonymousSink)"
shutdown_when_empty
wait_shutdown

if [ "$rainer_route" = "$rainer_changed" ]; then
	echo "FAIL: changed RainerScript expression produced the same ruleset fingerprint"
	error_exit 1
fi
if [ "$rainer_generated" = "$rainer_anonymous" ]; then
	echo "FAIL: explicit generated-looking name was treated as anonymous"
	error_exit 1
fi

generate_conf --yaml-only
cat >>"${TESTCONF_NM}.yaml" <<'YAMLEOF'

rulesets:
  - name: generatedSink
    actions:
      - type: omfile
        name: "action-0-builtin:omfile"
        file: "${RSYSLOG_OUT_LOG}"
  - name: anonymousSink
    actions:
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
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
yaml_generated="$(query_ruleset_fingerprint generatedSink)"
yaml_anonymous="$(query_ruleset_fingerprint anonymousSink)"
shutdown_when_empty
wait_shutdown

if [ "$yaml_generated" != "$rainer_generated" ] || [ "$yaml_anonymous" != "$rainer_anonymous" ] ||
	[ "$yaml_generated" = "$yaml_anonymous" ]; then
	echo "FAIL: action name provenance was not preserved across frontends"
	error_exit 1
fi

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
