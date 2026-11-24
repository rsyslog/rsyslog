#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The compiled traditional file template must match its documented string form:
# remove only the last message LF, then append one record LF. Fixed RFC3164
# metadata keeps every output byte deterministic. Binary files carry explicit
# octet-counted TCP frames so the helper cannot strip trailing LF arguments.
# The no-LF/one-LF/two-LF payloads disable receive-time stripping and escaping.
# Listener readiness comes from its port file. A separate acknowledgment after
# both output actions proves each payload arrived before the next send and the
# synchronized shutdown; exact file comparison includes trailing blank lines.
# An empty MSG separately checks zero-length safety and preserves the compiled
# formatter's existing separator space (not part of this LF correction).
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
global(parser.dropTrailingLFOnReception="off")
$EscapeControlCharactersOnReceive off
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="compiled" type="plugin" plugin="RSYSLOG_TraditionalFileFormat")
template(name="reference" type="string"
         string="%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n")
template(name="ack" type="string" string="delivered\n")
template(name="emptymsg" type="string" string="<%msg%>\n")
if $programname == "empty" then {
    action(type="omfile" template="compiled" file="'$RSYSLOG_DYNNAME'.empty")
    action(type="omfile" template="emptymsg" file="'$RSYSLOG_DYNNAME'.emptymsg")
    action(type="omfile" template="ack" file="'$RSYSLOG_DYNNAME'.ack")
}
if $programname == "tag" then {
    action(type="omfile" template="compiled" file="'$RSYSLOG_OUT_LOG'")
    action(type="omfile" template="reference" file="'$RSYSLOG_DYNNAME'.reference")
    action(type="omfile" template="ack" file="'$RSYSLOG_DYNNAME'.ack")
}
'
startup
for count in 0 1 2; do
    case "$count" in
        0) payload='<13>Mar  1 01:00:00 host tag: no-lf' ;;
        1) payload=$'<13>Mar  1 01:00:00 host tag: one-lf\n' ;;
        2) payload=$'<13>Mar  1 01:00:00 host tag: two-lf\n\n' ;;
    esac
    printf '%s %s' "${#payload}" "$payload" > "$RSYSLOG_DYNNAME.frame.$count"
    tcpflood -B -I "$RSYSLOG_DYNNAME.frame.$count"
    wait_file_lines "$RSYSLOG_DYNNAME.ack" "$((count + 1))"
done
payload='<13>Mar  1 01:00:00 host empty:'
printf '%s %s' "${#payload}" "$payload" > "$RSYSLOG_DYNNAME.frame.empty"
tcpflood -B -I "$RSYSLOG_DYNNAME.frame.empty"
wait_file_lines "$RSYSLOG_DYNNAME.ack" 4
shutdown_when_empty
wait_shutdown
printf 'Mar  1 01:00:00 host tag: no-lf\nMar  1 01:00:00 host tag: one-lf\nMar  1 01:00:00 host tag: two-lf\n\n' > "$RSYSLOG_DYNNAME.expected"
cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_DYNNAME.reference" || error_exit 1
cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG" || error_exit 1
printf 'Mar  1 01:00:00 host empty: \n' > "$RSYSLOG_DYNNAME.expected-empty"
printf '<>\n' > "$RSYSLOG_DYNNAME.expected-emptymsg"
cmp "$RSYSLOG_DYNNAME.expected-emptymsg" "$RSYSLOG_DYNNAME.emptymsg" || error_exit 1
cmp "$RSYSLOG_DYNNAME.expected-empty" "$RSYSLOG_DYNNAME.empty" || error_exit 1
exit_test
