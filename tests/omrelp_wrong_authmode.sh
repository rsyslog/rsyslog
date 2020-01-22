#!/bin/bash
# add 2018-09-13 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")

ruleset(name="ruleset") {
	action(type="omrelp" target="127.0.0.1" port="'$TCPFLOOD_PORT'"
	tls="on" tls.authMode="INVALID_AUTH_MODE" tls.caCert="tls-certs/ca.pem"
	tls.myCert="tls-certs/cert.pem" tls.myPrivKey="tls-certs/key.pem"
	tls.permittedPeer=["rsyslog-test-root-ca"])
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check --regex "omrelp.* invalid auth.*mode .*INVALID_AUTH_MODE"
exit_test
