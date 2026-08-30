#!/bin/bash
# Verify native-YAML parity for transactional C-style control escaping. BEL is
# the runtime oracle, and all records use one persistent octet-counted session.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"\
  config.reloadOnHUP: "on"\
  parser.escapeControlCharactersCStyle: "off"' "$yaml_conf"
cat >>"$yaml_conf" <<YAML_EOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
templates:
  - name: outfmt
    type: string
    string: "%msg%\\n"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: escapecstyle_sink
        file: "$RSYSLOG_OUT_LOG"
        template: outfmt
YAML_EOF
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1

printf -v payload '<167>Mar  6 16:57:54 host app: yaml-before-hup\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' yaml-before-hup#007' "$RSYSLOG_OUT_LOG"

sed 's/parser.escapeControlCharactersCStyle: "off"/parser.escapeControlCharactersCStyle: "on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML C-style escaping policy did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app: yaml-after-hup\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' yaml-after-hup' "$RSYSLOG_OUT_LOG"
content_check ' yaml-after-hup\a' "$RSYSLOG_OUT_LOG"
check_not_present 'yaml-after-hup#007'

sed -e 's/parser.escapeControlCharactersCStyle: "on"/parser.escapeControlCharactersCStyle: "off"/' \
	-e '/parser.escapeControlCharactersCStyle: "off"/a\  compactJsonString: "on"' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined YAML parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
printf -v payload '<167>Mar  6 16:57:54 host app: yaml-after-reject\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' yaml-after-reject' "$RSYSLOG_OUT_LOG"
content_check ' yaml-after-reject\a' "$RSYSLOG_OUT_LOG"
check_not_present 'yaml-after-reject#007'

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
