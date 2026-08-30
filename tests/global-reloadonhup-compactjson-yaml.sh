#!/bin/bash
# Verify native-YAML parity for transactional compactJsonString publication.
# The global field remains native YAML and enters the shared cnfobj/nvlst path;
# an unchanged RainerScript include supplies only the output rule. Exact JSON
# spacing is the active-value oracle.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf --yaml-only
yaml_conf="${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"\
  config.reloadOnHUP: "on"\
  compactJsonString: "off"' "$yaml_conf"
cat >"$RSYSLOG_DYNNAME.internal.conf" <<RS_EOF
template(name="outfmt" type="string" string="%msg%|%\$!%\\n")
set \$!obj!key = "value";
action(type="omfile" file="$RSYSLOG_OUT_LOG" template="outfmt")
RS_EOF
cat >>"$yaml_conf" <<YAML_EOF
include:
  - path: "$RSYSLOG_DYNNAME.internal.conf"
YAML_EOF
startup

injectmsg_literal '<167>Mar 10 01:00:00 host app: yaml-spaced-before-hup'
wait_content 'yaml-spaced-before-hup|{ "obj": { "key": "value" } }' "$RSYSLOG_OUT_LOG"

sed 's/compactJsonString: "off"/compactJsonString: "on"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML compactJsonString=on did not activate: $reload_status"
	error_exit 1
fi

injectmsg_literal '<167>Mar 10 01:00:00 host app: yaml-compact-after-hup'
wait_content 'yaml-compact-after-hup|{"obj":{"key":"value"}}' "$RSYSLOG_OUT_LOG"

# The rejected mixed update must not partially restore spaced serialization.
sed -e 's/compactJsonString: "on"/compactJsonString: "off"/' \
	-e '/compactJsonString: "off"/a\  oversizemsg.report: "off"' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined YAML base-policy change was not rejected atomically: $reload_status"
	error_exit 1
fi
injectmsg_literal '<167>Mar 10 01:00:00 host app: yaml-compact-after-reject'
wait_content 'yaml-compact-after-reject|{"obj":{"key":"value"}}' "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
exit_test
