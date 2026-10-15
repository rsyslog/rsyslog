#!/bin/bash
# Verify transactional compactJsonString publication from RainerScript.
# Each synchronized output record contains the runtime JSON representation, so
# exact spacing proves which committed value processed that message. The final
# mixed-change rejection must leave the previously committed format active.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" compactJsonString="off")
template(name="outfmt" type="string" string="%msg%|%$!%\n")
set $!obj!key = "value";
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

injectmsg_literal '<167>Mar 10 01:00:00 host app: spaced-before-hup'
wait_content 'spaced-before-hup|{ "obj": { "key": "value" } }' "$RSYSLOG_OUT_LOG"

sed 's/compactJsonString="off"/compactJsonString="on"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: compactJsonString=on did not activate: $reload_status"
	error_exit 1
fi

injectmsg_literal '<167>Mar 10 01:00:00 host app: compact-after-hup'
wait_content 'compact-after-hup|{"obj":{"key":"value"}}' "$RSYSLOG_OUT_LOG"

# Two base changes exceed this narrow live slice. A post-rejection record is
# the atomicity oracle: it must still use the compact representation.
sed 's/compactJsonString="on"/compactJsonString="off" oversizemsg.report="off"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined base-policy change was not rejected atomically: $reload_status"
	error_exit 1
fi
injectmsg_literal '<167>Mar 10 01:00:00 host app: compact-after-reject'
wait_content 'compact-after-reject|{"obj":{"key":"value"}}' "$RSYSLOG_OUT_LOG"

shutdown_when_empty
wait_shutdown
exit_test
