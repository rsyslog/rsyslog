#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The compiled modern file template must match the documented list template:
# remove exactly one terminal MSG LF, then append one record LF. A fixed
# RFC5424 timestamp (including precision/timezone), host, and tag make the byte
# oracle deterministic. Binary files carry octet-counted TCP frames so no
# helper argument unquoting can remove the payload's final LF bytes.
# Disable reception stripping/escaping. Listener port-file readiness plus an
# acknowledgment after both output actions proves each payload arrived before
# the next send and synchronized shutdown. Compare exact bytes, including blank
# lines. Reference/no-LF/empty controls run before the trailing-LF regression.
# Empty MSG checks zero-length safety and preserves the compiled formatter's
# existing separator space; changing empty-message formatting is out of scope.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
global(parser.dropTrailingLFOnReception="off")
$EscapeControlCharactersOnReceive off
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="reference" type="list") {
    property(name="timereported" dateFormat="rfc3339")
    constant(value=" ")
    property(name="hostname")
    constant(value=" ")
    property(name="syslogtag")
    property(name="msg" spIfNo1stSp="on")
    property(name="msg" dropLastLf="on")
    constant(value="\n")
}
template(name="ack" type="string" string="delivered\n")
template(name="emptymsg" type="string" string="<%msg%>\n")
if $programname == "empty" then {
    action(type="omfile" template="RSYSLOG_FileFormat" file="'$RSYSLOG_DYNNAME'.empty")
    action(type="omfile" template="emptymsg" file="'$RSYSLOG_DYNNAME'.emptymsg")
    action(type="omfile" template="ack" file="'$RSYSLOG_DYNNAME'.ack")
}
if $programname == "tag" then {
    action(type="omfile" template="RSYSLOG_FileFormat" file="'$RSYSLOG_OUT_LOG'")
    action(type="omfile" template="reference" file="'$RSYSLOG_DYNNAME'.reference")
    action(type="omfile" template="ack" file="'$RSYSLOG_DYNNAME'.ack")
}
'
if ! ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
    > "$RSYSLOG_DYNNAME.config-check.log" 2>&1; then
    cat "$RSYSLOG_DYNNAME.config-check.log"
    error_exit 1
fi
startup
for count in 0 1 2; do
    case "$count" in
        0) payload='<13>1 2026-03-01T01:00:00.123456+02:00 host tag - - - no-lf' ;;
        1) payload=$'<13>1 2026-03-01T01:00:00.123456+02:00 host tag - - - one-lf\n' ;;
        2) payload=$'<13>1 2026-03-01T01:00:00.123456+02:00 host tag - - - two-lf\n\n' ;;
    esac
    printf '%s %s' "${#payload}" "$payload" > "$RSYSLOG_DYNNAME.frame.$count"
    tcpflood -B -I "$RSYSLOG_DYNNAME.frame.$count"
    wait_file_lines "$RSYSLOG_DYNNAME.ack" "$((count + 1))"
done
payload='<13>1 2026-03-01T01:00:00.123456+02:00 host empty - - - '
printf '%s %s' "${#payload}" "$payload" > "$RSYSLOG_DYNNAME.frame.empty"
tcpflood -B -I "$RSYSLOG_DYNNAME.frame.empty"
wait_file_lines "$RSYSLOG_DYNNAME.ack" 4
shutdown_when_empty
wait_shutdown
printf '2026-03-01T01:00:00.123456+02:00 host tag no-lf\n2026-03-01T01:00:00.123456+02:00 host tag one-lf\n2026-03-01T01:00:00.123456+02:00 host tag two-lf\n\n' > "$RSYSLOG_DYNNAME.expected"
cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_DYNNAME.reference" || error_exit 1
printf '2026-03-01T01:00:00.123456+02:00 host tag no-lf\n' > "$RSYSLOG_DYNNAME.expected-first"
head -n 1 "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.first"
cmp "$RSYSLOG_DYNNAME.expected-first" "$RSYSLOG_DYNNAME.first" || error_exit 1
printf '2026-03-01T01:00:00.123456+02:00 host empty \n' > "$RSYSLOG_DYNNAME.expected-empty"
printf '<>\n' > "$RSYSLOG_DYNNAME.expected-emptymsg"
cmp "$RSYSLOG_DYNNAME.expected-emptymsg" "$RSYSLOG_DYNNAME.emptymsg" || error_exit 1
cmp "$RSYSLOG_DYNNAME.expected-empty" "$RSYSLOG_DYNNAME.empty" || error_exit 1
echo 'reference, no-LF, and empty-MSG controls: PASS'
cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG" || error_exit 1
exit_test
