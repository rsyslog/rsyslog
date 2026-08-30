#!/bin/bash
# Verify transactional parser.escape8BitCharactersOnReceive publication from
# RainerScript. Byte 0xe9 becomes #351 while enabled and remains raw while
# disabled; octet counting keeps all records on one persistent TCP session.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.escape8BitCharactersOnReceive="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1

printf -v payload '<167>Mar  6 16:57:54 host app: before-hup\xe9\n'
payload_len=$(printf '%s' "$payload" | wc -c)
printf '%s %s' "$payload_len" "$payload" >&9 || error_exit 1
wait_content ' before-hup#351' "$RSYSLOG_OUT_LOG"

sed 's/parser.escape8BitCharactersOnReceive="on"/parser.escape8BitCharactersOnReceive="off"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.escape8BitCharactersOnReceive=off did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app: after-hup\xe9\n'
payload_len=$(printf '%s' "$payload" | wc -c)
printf '%s %s' "$payload_len" "$payload" >&9 || error_exit 1
wait_content $' after-hup\xe9' "$RSYSLOG_OUT_LOG"
check_not_present 'after-hup#351'

# A mixed global candidate must not partially re-enable 8-bit escaping.
sed 's/parser.escape8BitCharactersOnReceive="off"/parser.escape8BitCharactersOnReceive="on" compactJsonString="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
printf -v payload '<167>Mar  6 16:57:54 host app: after-reject\xe9\n'
payload_len=$(printf '%s' "$payload" | wc -c)
printf '%s %s' "$payload_len" "$payload" >&9 || error_exit 1
wait_content $' after-reject\xe9' "$RSYSLOG_OUT_LOG"
check_not_present 'after-reject#351'

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
