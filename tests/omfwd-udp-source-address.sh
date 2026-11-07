#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Exercise explicit-source UDP socket creation and teardown without a preload,
# including sanitizer builds. A real loopback receiver must see exactly one
# fixed payload from the configured IPv4 source. A bound port file and bounded
# line predicate synchronize delivery; normal shutdown checks socket cleanup.
. ${srcdir:=.}/diag.sh init
require_plugin imudp
portfile="${RSYSLOG_DYNNAME}.udp.port"
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$portfile'")
template(name="received" type="string" string="%fromhost-ip% %msg%\n")
if $programname == "sourcecheck" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="received")
'
startup
receiver_port=""
assign_file_content receiver_port "$portfile"
generate_conf 2
add_conf '
template(name="wire" type="string" string="<13>1 2000-01-01T00:00:00Z host sourcecheck - - - literal-source\n")
if $msg contains "msgnum:" then action(type="omfwd" protocol="udp" target="127.0.0.1" port="'$receiver_port'" Address="127.0.0.1" template="wire")
' 2
startup 2
injectmsg2 0 1
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown
export EXPECTED="127.0.0.1 literal-source"
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
