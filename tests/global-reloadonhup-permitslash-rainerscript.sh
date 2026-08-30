#!/bin/bash
# Verify transactional parser.permitSlashInProgramName publication from
# RainerScript. The programname on one persistent TCP session changes from
# app to app/foo; a mixed global candidate must leave the live value intact.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.permitSlashInProgramName="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%programname%:%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1

printf -v payload '<167>Mar  6 16:57:54 host app/foo[123]: before-hup\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content 'app: before-hup' "$RSYSLOG_OUT_LOG"

sed 's/parser.permitSlashInProgramName="off"/parser.permitSlashInProgramName="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.permitSlashInProgramName=on did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app/foo[123]: after-hup\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content 'app/foo: after-hup' "$RSYSLOG_OUT_LOG"

sed 's/parser.permitSlashInProgramName="on"/parser.permitSlashInProgramName="off" compactJsonString="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
printf -v payload '<167>Mar  6 16:57:54 host app/foo[123]: after-reject\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content 'app/foo: after-reject' "$RSYSLOG_OUT_LOG"

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
