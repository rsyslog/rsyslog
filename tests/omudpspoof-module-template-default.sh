#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify that a modern module-level template default also reaches the legacy
# selector action. One injected message must produce three loopback UDP records:
# one using the module default through the legacy selector, one using that same
# default through a modern action, and one using an explicit action template.
# The sender input uses its own ruleset and the receiver filters on the
# injected-message marker so startup diagnostics cannot enter the oracle.
# The receiver line-count wait uses the harness timeout to fail if a packet is
# missing, and proves delivery without a fixed delay; marker counts distinguish
# the fallback from both positive controls.
echo This test must be run as root [raw socket access required]
if [ "$EUID" -ne 0 ]; then
	exit 77
fi

. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
export TCPFLOOD_EXTRA_OPTS="-b1 -W1"
export PORT_UDP_FILE="${RSYSLOG_DYNNAME}.imudp_port"

generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0"
      listenPortFileName="'"$PORT_UDP_FILE"'")
template(name="outfmt" type="string" string="%msg%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
startup
assign_file_content PORT_UDP "$PORT_UDP_FILE"

generate_conf 2
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

template(name="spoofaddr" type="string" string="127.0.0.1")
template(name="moduleDefault" type="string"
         string="<%PRI%>%TIMESTAMP% %HOSTNAME% testtag: MODULE_DEFAULT_MARKER:%msg%\n")
template(name="explicitAction" type="string"
         string="<%PRI%>%TIMESTAMP% %HOSTNAME% testtag: EXPLICIT_ACTION_MARKER:%msg%\n")
module(load="../plugins/omudpspoof/.libs/omudpspoof" template="moduleDefault")

$ActionOMUDPSpoofTargetHost 127.0.0.1
$ActionOMUDPSpoofTargetPort '"$PORT_UDP"'
$ActionOMUDPSpoofSourceNameTemplate spoofaddr

$RuleSet spoof_test
:msg, contains, "msgnum:" action(type="omudpspoof" target="127.0.0.1" port="'"$PORT_UDP"'"
       sourceTemplate="spoofaddr")
:msg, contains, "msgnum:" action(type="omudpspoof" target="127.0.0.1" port="'"$PORT_UDP"'"
       sourceTemplate="spoofaddr" template="explicitAction")
:msg, contains, "msgnum:" :omudpspoof:
$RuleSet RSYSLOG_DefaultRuleset
input(type="imtcp" port="'"$TCPFLOOD_PORT"'" ruleset="spoof_test")
' 2
startup 2

tcpflood -m "$NUMMESSAGES"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3 "$TB_TEST_TIMEOUT"
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_count_check "EXPLICIT_ACTION_MARKER:" 1 "$RSYSLOG_OUT_LOG"
content_count_check "msgnum:00000000:" 3 "$RSYSLOG_OUT_LOG"
content_count_check "MODULE_DEFAULT_MARKER:" 2 "$RSYSLOG_OUT_LOG"
exit_test
