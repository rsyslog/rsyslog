#!/bin/bash
# rgerhards, 2011-04-04
# testing sending and receiving via TLS with anon auth using bare ipv6, no SNI
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-ipv6-available
export NUMMESSAGES=25000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"

# start up the instances
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir'/testsuites/x.509/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/testsuites/x.509/client-cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/testsuites/x.509/client-key.pem"
	defaultNetstreamDriver="gtls"
	debug.whitelist="on"
	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(load="../plugins/imtcp/.libs/imtcp" maxSessions="1100"
       streamDriver.mode="1" streamDriver.authMode="anon")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
#unset RSYSLOG_DEBUG # suppress this debug log, if you want
export PORT_RCVR=$TCPFLOOD_PORT
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)" # TODO: move to diag.sh
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="gtls"
)

# set up the action
$DefaultNetstreamDriver gtls # use gtls netstream driver
$ActionSendStreamDriverMode 1 # require TLS for the connection
$ActionSendStreamDriverAuthMode anon
*.*	@@[::1]:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2

# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check
exit_test
