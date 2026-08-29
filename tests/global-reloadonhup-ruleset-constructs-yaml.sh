#!/bin/bash
# Verify that private YAML ruleset materialization preserves foreach and both
# capture-only legacy filter forms. Each HUP changes only the existing ruleset,
# reuses the same named action, and must publish exactly one new generation.
# A persistent TCP stream plus a visible record after every cutover proves the
# prepared tree is executable and that action ownership survives retirement.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
require_plugin mmjsonparse
generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "$yaml_conf"

write_yaml_config() {
	local body=$1
	cat >>"$yaml_conf" <<YAML_EOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: main
rulesets:
  - name: main
$body
YAML_EOF
}

cat >>"$yaml_conf" <<YAML_EOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: main
rulesets:
  - name: main
    script: |
      action(type="mmjsonparse" name="construct_parser")
      if \$msg contains "startup-live" then
        action(type="omfile" name="construct_sink" file="$RSYSLOG_OUT_LOG")
YAML_EOF

startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: startup-live\n' >&9 || error_exit 1
wait_content 'startup-live' "$RSYSLOG_OUT_LOG"

# The collection and iterator are private deep clones. The named action in the
# nested body must bind to the active action without transferring it on abort.
sed -n '1,/^modules:/p' "$yaml_conf" | sed '$d' >"$yaml_conf.candidate"
mv "$yaml_conf.candidate" "$yaml_conf"
write_yaml_config '    script: |
      action(type="mmjsonparse" name="construct_parser")
      foreach ($.item in $!items) do {
        if $.item == "second" then
          action(type="omfile" name="construct_sink" file="'$RSYSLOG_OUT_LOG'")
      }'
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML foreach plan did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: @cee: {"items":["first","second"],"text":"foreach-live"}\n' >&9 || error_exit 1
wait_content 'foreach-live' "$RSYSLOG_OUT_LOG"

# A property filter in YAML script creates S_RELOAD_PROPFILT. Lowering must
# decode it against the active policy before optimizer and action binding.
sed -n '1,/^modules:/p' "$yaml_conf" | sed '$d' >"$yaml_conf.candidate"
mv "$yaml_conf.candidate" "$yaml_conf"
write_yaml_config '    script: |
      action(type="mmjsonparse" name="construct_parser")
      :msg, contains, "property-live" {
        action(type="omfile" name="construct_sink" file="'$RSYSLOG_OUT_LOG'")
      }'
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML property-filter plan did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: property-live\n' >&9 || error_exit 1
wait_content 'property-live' "$RSYSLOG_OUT_LOG"

# The PRI form exercises the other capture-only node and its decoded pmask.
sed -n '1,/^modules:/p' "$yaml_conf" | sed '$d' >"$yaml_conf.candidate"
mv "$yaml_conf.candidate" "$yaml_conf"
write_yaml_config '    script: |
      action(type="mmjsonparse" name="construct_parser")
      *.* {
        action(type="omfile" name="construct_sink" file="'$RSYSLOG_OUT_LOG'")
      }'
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML priority-filter plan did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: priority-live\n' >&9 || error_exit 1
wait_content 'priority-live' "$RSYSLOG_OUT_LOG"

exec 9>&-
shutdown_when_empty
wait_shutdown
content_check 'startup-live' "$RSYSLOG_OUT_LOG"
content_check 'foreach-live' "$RSYSLOG_OUT_LOG"
content_check 'property-live' "$RSYSLOG_OUT_LOG"
content_check 'priority-live' "$RSYSLOG_OUT_LOG"
exit_test
