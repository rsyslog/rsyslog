#!/bin/bash
# Regression test for lazy UDP source-address handling used by headerless
# messages.  The fromhost-ip property resolves through the sockaddr stored at
# receive time.  The oracle is that normal UDP loopback traffic produces stable
# loopback source properties and never exposes the internal ?error.obtaining.ip?
# DNS-cache sentinel reported in issue #5461.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

generate_conf
add_conf '
global(net.enableDNS="off")

module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" type="string" string="host=%hostname% from=%fromhost-ip% raw=%rawmsg%\n")

if $rawmsg contains "headerless message" then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

startup
tcpflood -m "$NUMMESSAGES" -T udp -M "\"headerless message\""
shutdown_when_empty
wait_shutdown

wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
content_check "host=headerless from=127.0.0.1 raw=headerless message" "$RSYSLOG_OUT_LOG"
check_not_present "?error.obtaining.ip?" "$RSYSLOG_OUT_LOG"

exit_test
