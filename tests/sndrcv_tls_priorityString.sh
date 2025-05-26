#!/bin/bash
# Pascal Withopf, 2017-07-25
# testing sending and receiving via TLS with anon auth
# NOTE: When this test fails, it could be due to the priorityString being outdated!
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2500
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
# start up the instances
export RSYSLOG_DEBUGLOG="log"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
# certificates
global(
	defaultNetstreamDriverCAFile="'$srcdir'/testsuites/x.509/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/testsuites/x.509/client-cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/testsuites/x.509/client-key.pem"
	defaultNetstreamDriver="gtls"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls" StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" gnutlspriorityString="NORMAL:-MD5")
input(type="imtcp" port="'$PORT_RCVR'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum" then {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup 
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)" # TODO: move to diag.sh
add_conf '
#certificates
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfwd" Target="127.0.0.1" port="'$PORT_RCVR'" Protocol="tcp" streamdriver="gtls"
	StreamDriverAuthMode="anon" StreamDriverMode="1"
	gnutlsprioritystring="NORMAL:-MD5")
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m$NUMMESSAGES -i1
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2

wait_file_lines
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 $NUMMESSAGES
exit_test
