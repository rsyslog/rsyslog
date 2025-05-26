#!/bin/bash
# alorbach, 2019-11-27
#	Required Packages
#	pip install pysnmp
#
#	Ubuntu 18 Packages needed
#	apt install snmp libsnmp-dev snmp-mibs-downloader
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export TESTMESSAGES=10

generate_conf
export PORT_SNMP="$(get_free_port)"
# Start SNMP Trap Receiver
snmp_start_trapreceiver ${PORT_SNMP} ${RSYSLOG_OUT_LOG}

add_conf '
module(	load="../plugins/imtcp/.libs/imtcp")
input(	type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

module(	load="../plugins/omsnmp/.libs/omsnmp" )

# set up the action
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(	type="omsnmp"
					name="name"
					server="127.0.0.1"
					port="'${PORT_SNMP}'"
					version="0"
					community="public"
					enterpriseOID="1.3.6.1.2.1.192.0.1"
					messageOID="1.3.6.1.2.1.192.1.2.1.11"
					TrapType="6"
					specificType="0"
				)
'
startup
tcpflood -p$TCPFLOOD_PORT -m${TESTMESSAGES}
netstat -l

shutdown_when_empty
wait_shutdown
snmp_stop_trapreceiver
content_count_check "msgnum:" ${TESTMESSAGES}
exit_test
