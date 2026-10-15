#!/bin/bash
# Verify pure native-YAML parity for transactional trailing-CR parser policy.
# Global, input, template, ruleset, and action all enter the shared cnfobj/nvlst
# path directly. Exact #015 presence is the runtime oracle.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"\
  config.reloadOnHUP: "on"\
  parser.dropTrailingCROnReception: "off"' "$yaml_conf"
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
        name: droptrailingcr_sink
        file: "$RSYSLOG_OUT_LOG"
        template: outfmt
YAML_EOF
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: yaml-before-hup\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' yaml-before-hup#015' "$RSYSLOG_OUT_LOG"

sed 's/parser.dropTrailingCROnReception: "off"/parser.dropTrailingCROnReception: "on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML trailing-CR policy did not activate: $reload_status"
	error_exit 1
fi

cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: yaml-after-hup\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' yaml-after-hup' "$RSYSLOG_OUT_LOG"
check_not_present 'yaml-after-hup#015'

sed -e 's/parser.dropTrailingCROnReception: "on"/parser.dropTrailingCROnReception: "off"/' \
	-e '/parser.dropTrailingCROnReception: "off"/a\  compactJsonString: "on"' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined YAML parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: yaml-after-reject\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' yaml-after-reject' "$RSYSLOG_OUT_LOG"
check_not_present 'yaml-after-reject#015'

shutdown_when_empty
wait_shutdown
exit_test
