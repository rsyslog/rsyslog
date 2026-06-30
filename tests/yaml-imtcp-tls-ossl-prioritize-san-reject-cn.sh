#!/bin/bash
# Verify YAML parsing of the imtcp StreamDriver.PrioritizeSAN parameter for
# OpenSSL x509/name authorization. The fixture certificate has
# CN=rsyslog-client and DNS SAN testbench.rsyslog.com. The listener is loaded
# from YAML with PrioritizeSAN enabled and must reject CN-only authorization
# when a SAN exists. The oracle is the receiver-side "peer name not authorized"
# diagnostic routed by the testbench action after a checked tcpflood attempt
# and synchronized shutdown.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=3
generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    StreamDriver.Name: "ossl"'
add_yaml_conf '    StreamDriver.Mode: "1"'
add_yaml_conf '    StreamDriver.AuthMode: "x509/name"'
add_yaml_conf '    StreamDriver.PrioritizeSAN: "on"'
add_yaml_conf "    StreamDriver.CAFile: \"${srcdir}/tls-certs/ca.pem\""
add_yaml_conf "    StreamDriver.CertFile: \"${srcdir}/tls-certs/cert.pem\""
add_yaml_conf "    StreamDriver.KeyFile: \"${srcdir}/tls-certs/key.pem\""
add_yaml_conf '    PermittedPeer: ["rsyslog-client"]'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf "    listenPortFileName: \"${RSYSLOG_DYNNAME}.tcpflood_port\""
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood --check-only -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
content_check "peer name not authorized"
check_not_present "msgnum:"
exit_test
