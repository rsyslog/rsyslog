#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify transactional parser.controlCharacterEscapePrefix publication from
# RainerScript. BEL changes from #007 to @007 on one persistent TCP session;
# a mixed global candidate must leave the live prefix intact.
# Empty/multi-byte candidates must reject without advancing the generation.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(config.reloadOnHUP="on" parser.controlCharacterEscapePrefix="#")
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

sed 's/parser.controlCharacterEscapePrefix="#"/parser.controlCharacterEscapePrefix="@"/' \
	"$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
	echo "FAIL: parser.controlCharacterEscapePrefix=@ did not activate: $reload_status"
	error_exit 1
fi

printf -v payload '<167>Mar  6 16:57:54 host app: after-hup\a\n'
printf '%s %s' "${#payload}" "$payload" >&9 || error_exit 1
wait_content ' after-hup@007' "$RSYSLOG_OUT_LOG"

# Empty and multi-byte prefixes are malformed, not partial scalar updates.
cp "$CONF_FILE" "$CONF_FILE.active"
for bad_prefix in '' 'ab'; do
    sed "s/parser.controlCharacterEscapePrefix=\"@\"/parser.controlCharacterEscapePrefix=\"$bad_prefix\"/" \
        "$CONF_FILE.active" >"$CONF_FILE"
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=candidate_normalization_unsupported active_generation=2"* ]]; then
        echo "FAIL: malformed prefix was not rejected: $reload_status"
        error_exit 1
    fi
done
cp "$CONF_FILE.active" "$CONF_FILE"

sed 's/parser.controlCharacterEscapePrefix="@"/parser.controlCharacterEscapePrefix="#" compactJsonString="on"/' \
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
wait_content ' after-reject@007' "$RSYSLOG_OUT_LOG"

exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
