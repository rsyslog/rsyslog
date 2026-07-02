#!/bin/bash
# Verify modern allowedSender ACLs for imtcp and imudp. Module-level
# allowedSender values act as defaults, input-level values override them, and
# /0 masks remain non-allow-all. The oracle is deterministic: allowed inputs
# must produce the expected file lines, blocked inputs must not create their
# ruleset output files, and rsyslog must emit the disallowed-sender diagnostics.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imudp

export NUMMESSAGES=3
TCP_ALLOW_LOG="${RSYSLOG_DYNNAME}.tcp-allow.log"
UDP_ALLOW_LOG="${RSYSLOG_DYNNAME}.udp-allow.log"
TCP_BLOCK_LOG="${RSYSLOG_DYNNAME}.tcp-block.must-not-exist"
TCP_ZERO_LOG="${RSYSLOG_DYNNAME}.tcp-zero.must-not-exist"
UDP_BLOCK_LOG="${RSYSLOG_DYNNAME}.udp-block.must-not-exist"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp" allowedSender=["128.66.0.0/16"])
module(load="../plugins/imudp/.libs/imudp" allowedSender=["128.66.0.0/16"])

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcp_block_port" ruleset="tcp_block")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcp_allow_port"
      allowedSender=["127.0.0.1/16","[::1]"] ruleset="tcp_allow")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcp_zero_port"
      allowedSender=["128.66.0.0/0"] ruleset="tcp_zero")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.udp_block_port"
      ruleset="udp_block")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.udp_allow_port"
      allowedSender=["127.0.0.1/16"] ruleset="udp_allow")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="tcp_block") {
	action(type="omfile" template="outfmt" file="'$TCP_BLOCK_LOG'")
}
ruleset(name="tcp_allow") {
	action(type="omfile" template="outfmt" file="'$TCP_ALLOW_LOG'")
}
ruleset(name="tcp_zero") {
	action(type="omfile" template="outfmt" file="'$TCP_ZERO_LOG'")
}
ruleset(name="udp_block") {
	action(type="omfile" template="outfmt" file="'$UDP_BLOCK_LOG'")
}
ruleset(name="udp_allow") {
	action(type="omfile" template="outfmt" file="'$UDP_ALLOW_LOG'")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

assign_file_content TCP_BLOCK_PORT "${RSYSLOG_DYNNAME}.tcp_block_port"
assign_file_content TCP_ALLOW_PORT "${RSYSLOG_DYNNAME}.tcp_allow_port"
assign_file_content TCP_ZERO_PORT "${RSYSLOG_DYNNAME}.tcp_zero_port"
assign_file_content UDP_BLOCK_PORT "${RSYSLOG_DYNNAME}.udp_block_port"
assign_file_content UDP_ALLOW_PORT "${RSYSLOG_DYNNAME}.udp_allow_port"

tcpflood --check-only -p"$TCP_BLOCK_PORT" -m1 -M "msgnum:tcp-block"
tcpflood -p"$TCP_ALLOW_PORT" -m"$NUMMESSAGES" -M "msgnum:tcp-allow"
tcpflood --check-only -p"$TCP_ZERO_PORT" -m1 -M "msgnum:tcp-zero"
tcpflood -Tudp -p"$UDP_BLOCK_PORT" -m1 -M "msgnum:udp-block"
tcpflood -Tudp -p"$UDP_ALLOW_PORT" -m"$NUMMESSAGES" -M "msgnum:udp-allow"

wait_file_lines "$TCP_ALLOW_LOG" "$NUMMESSAGES" 100
wait_file_lines "$UDP_ALLOW_LOG" "$NUMMESSAGES" 100
shutdown_when_empty
wait_shutdown

content_check "You can not specify 0 bits of the netmask"
content_check --regex "connection request from disallowed sender .* discarded"
content_check "imudp: UDP message from disallowed sender discarded"
check_file_not_exists "$TCP_BLOCK_LOG"
check_file_not_exists "$TCP_ZERO_LOG"
check_file_not_exists "$UDP_BLOCK_LOG"
exit_test
