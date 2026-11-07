#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify that YAML module-level template configuration is the default for a
# modern omudpspoof action with no action-level template. A dedicated ruleset
# keeps startup diagnostics outside the one-message loopback receiver oracle,
# whose output action also filters on the injected-message marker. It observes
# the distinctive payload after one injected message. The
# bounded receiver line-count wait uses the harness timeout to fail if it is
# missing instead of relying on a fixed delay.
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
include(file="'"${RSYSLOG_DYNNAME}"'.yaml")
' 2
cp -f "$srcdir/testsuites/yaml-omudpspoof-module-template-default.yaml" "${RSYSLOG_DYNNAME}.yaml"
sed -i \
	-e "s|@TCPFLOOD_PORT@|$TCPFLOOD_PORT|g" \
	-e "s|@PORT_UDP@|$PORT_UDP|g" \
	"${RSYSLOG_DYNNAME}.yaml"
startup 2

tcpflood -m "$NUMMESSAGES"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1 "$TB_TEST_TIMEOUT"
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_count_check "YAML_MODULE_DEFAULT_MARKER:" 1 "$RSYSLOG_OUT_LOG"
content_count_check "msgnum:00000000:" 1 "$RSYSLOG_OUT_LOG"
exit_test
