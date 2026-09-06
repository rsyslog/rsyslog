#!/bin/bash
# Successful TLS sessions must not produce a normal "no shared curve" diagnostic.
# OpenSSL can return zero on the client even after ECDHE negotiation. The RSA
# wrapper also covers successful TLS 1.2 without a key-exchange group on either
# side. Message delivery proves handshake success; synchronized shutdown drains
# internal diagnostics before checking both configured omfile destinations.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=100
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
: "${OSSL_TEST_CIPHER:=ECDHE-RSA-AES256-GCM-SHA384}"
OSSL_TEST_COMMANDS="MinProtocol=TLSv1.2;MaxProtocol=TLSv1.2;CipherString=$OSSL_TEST_CIPHER"
generate_conf
add_conf '
global(
    defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
    defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
    defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)
module(load="../plugins/imtcp/.libs/imtcp"
    StreamDriver.Name="ossl"
    StreamDriver.Mode="1"
    StreamDriver.AuthMode="anon"
    gnutlsPriorityString="'$OSSL_TEST_COMMANDS'")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$RSYSLOG_DYNNAME'.rcvr_port")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
    action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
action(type="omfile" file="'$RSYSLOG_DYNNAME'.receiver-diagnostics")
'
startup
assign_file_content PORT_RCVR "$RSYSLOG_DYNNAME.rcvr_port"
generate_conf 2
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")
action(type="omfile" file="'$RSYSLOG_DYNNAME'.sender-diagnostics")
if $msg contains "msgnum:" then
    action(type="omfwd" protocol="tcp" target="127.0.0.1"
        port="'$PORT_RCVR'" StreamDriver="ossl"
        StreamDriverMode="1" StreamDriverAuthMode="x509/certvalid"
        gnutlsPriorityString="'$OSSL_TEST_COMMANDS'")
' 2
startup 2
injectmsg2
wait_file_lines
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown
seq_check
content_check "msgnum:" "$RSYSLOG_DYNNAME.receiver-diagnostics"
content_check "msgnum:" "$RSYSLOG_DYNNAME.sender-diagnostics"
check_not_present "no shared curve" "$RSYSLOG_DYNNAME.receiver-diagnostics"
check_not_present "no shared curve" "$RSYSLOG_DYNNAME.sender-diagnostics"
exit_test
