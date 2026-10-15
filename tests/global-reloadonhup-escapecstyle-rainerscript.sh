#!/bin/bash
# Verify transactional parser.escapeControlCharactersCStyle publication from
# RainerScript. BEL is #007 while disabled and \a while enabled; all records
# use one persistent octet-counted TCP session.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.escapeControlCharactersCStyle="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1

printf -v payload '<167>Mar  6 16:57:54 host app: before-hup\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' before-hup#007' "$RSYSLOG_OUT_LOG"

sed 's/parser.escapeControlCharactersCStyle="off"/parser.escapeControlCharactersCStyle="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.escapeControlCharactersCStyle=on did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app: after-hup\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' after-hup' "$RSYSLOG_OUT_LOG"
content_check ' after-hup\a' "$RSYSLOG_OUT_LOG"
check_not_present 'after-hup#007'

# A mixed global candidate must not partially restore octal escaping.
sed 's/parser.escapeControlCharactersCStyle="on"/parser.escapeControlCharactersCStyle="off" compactJsonString="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
printf -v payload '<167>Mar  6 16:57:54 host app: after-reject\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' after-reject' "$RSYSLOG_OUT_LOG"
content_check ' after-reject\a' "$RSYSLOG_OUT_LOG"
check_not_present 'after-reject#007'

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
