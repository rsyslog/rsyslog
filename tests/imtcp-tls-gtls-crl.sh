#!/bin/bash
# Test CRL handling with GnuTLS TLS driver.
# Phase 1: Valid (non-expired) CRL with a non-revoked cert - communication should succeed.
# Phase 2: Expired CRL - connection should be rejected and no messages received.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

##############################
# Phase 1: Valid CRL - expect success
##############################
export NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid"
	StreamDriver.PermitExpiredCerts="off"
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
export PORT_RCVR=$TCPFLOOD_PORT
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

$ActionSendStreamDriverMode 1
$ActionSendStreamDriverAuthMode anon
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

injectmsg2

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

seq_check
printf 'Phase 1 PASSED: valid CRL, communication succeeded\n'

# Reset for phase 2
rm -f "$RSYSLOG_OUT_LOG"
unset QUEUE_EMPTY_CHECK_FUNC

##############################
# Phase 2: Expired CRL - expect rejection, no messages received
##############################
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver2.debuglog"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl-expired.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid"
	StreamDriver.PermitExpiredCerts="off"
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
export PORT_RCVR=$TCPFLOOD_PORT
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender2.debuglog"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultnetstreamdriverCRLfile="'$srcdir/testsuites/x.509/crl.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert-new.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

$ActionSendStreamDriverMode 1
$ActionSendStreamDriverAuthMode anon
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

injectmsg2

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

content_check --regex "CRL.*has expired"
assert_content_missing "msgnum:"
printf 'Phase 2 PASSED: expired CRL, connection rejected, no messages received\n'

exit_test
