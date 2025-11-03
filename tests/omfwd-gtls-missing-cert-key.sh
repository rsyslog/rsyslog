#!/bin/bash
# Test for gnutls loggedWarnings functionality with omfwd
# This test verifies that warnings for missing cert/key files are logged only once
# even when the action retries multiple times (loggedWarnings mechanism)
. ${srcdir:=.}/diag.sh init

export PORT_RCVR="$(get_free_port)"
export RS_REDIR=">${RSYSLOG_DYNNAME}.rsyslog.log 2>&1"

generate_conf
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")

action(type="omfwd" protocol="tcp" target="127.0.0.1" port="'$PORT_RCVR'"
	StreamDriver="gtls"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/name"
	action.resumeRetryCount="-1"
	action.resumeInterval="10")
'
startup
sleep 30
shutdown_immediate
wait_shutdown

content_count_check "warning: certificate file is not set" 1 ${RSYSLOG_DYNNAME}.rsyslog.log
content_count_check "warning: key file is not set" 1 ${RSYSLOG_DYNNAME}.rsyslog.log

exit_test
