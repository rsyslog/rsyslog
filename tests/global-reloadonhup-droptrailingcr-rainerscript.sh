#!/bin/bash
# Verify transactional parser.dropTrailingCROnReception publication from
# RainerScript. A trailing CR is escaped as #015 while disabled and absent
# after activation. The rejected mixed update must leave dropping enabled.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.dropTrailingCROnReception="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"

cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: before-hup\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' before-hup#015' "$RSYSLOG_OUT_LOG"

sed 's/parser.dropTrailingCROnReception="off"/parser.dropTrailingCROnReception="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.dropTrailingCROnReception=on did not activate: $reload_status"
	error_exit 1
fi

cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: after-hup\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' after-hup' "$RSYSLOG_OUT_LOG"
check_not_present 'after-hup#015'

# Two base changes exceed this narrow live slice. The following record proves
# that the rejected generation did not partially turn CR dropping off.
sed 's/parser.dropTrailingCROnReception="on"/parser.dropTrailingCROnReception="off" compactJsonString="on"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: combined parser/global change was not rejected atomically: $reload_status"
	error_exit 1
fi
cr_msg=$(printf '"<167>Mar  6 16:57:54 host app: after-reject\r"')
tcpflood -m1 -M "$cr_msg"
wait_content ' after-reject' "$RSYSLOG_OUT_LOG"
check_not_present 'after-reject#015'

shutdown_when_empty
wait_shutdown
exit_test
