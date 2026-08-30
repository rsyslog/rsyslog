#!/bin/bash
# Verify transactional parser.spaceLFOnReceive publication from RainerScript.
# An embedded LF is #012 while spacing is off and a literal space while on;
# octet-counted framing keeps all records on one persistent TCP session.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.spaceLFOnReceive="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1

printf -v payload '<167>Mar  6 16:57:54 host app: before-hup\nembedded-before\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' before-hup#012embedded-before' "$RSYSLOG_OUT_LOG"

sed 's/parser.spaceLFOnReceive="off"/parser.spaceLFOnReceive="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.spaceLFOnReceive=on did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app: after-hup\nembedded-after\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' after-hup embedded-after' "$RSYSLOG_OUT_LOG"
check_not_present 'after-hup#012embedded-after'

# A mixed global candidate must not partially turn LF spacing back off.
sed 's/parser.spaceLFOnReceive="on"/parser.spaceLFOnReceive="off" compactJsonString="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
printf -v payload '<167>Mar  6 16:57:54 host app: after-reject\nembedded-reject\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' after-reject embedded-reject' "$RSYSLOG_OUT_LOG"
check_not_present 'after-reject#012embedded-reject'

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
