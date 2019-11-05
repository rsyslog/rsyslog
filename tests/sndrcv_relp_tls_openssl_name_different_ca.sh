#!/bin/bash
# added 2018-09-13 by PascalWithopf
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"

require_relpEngineSetTLSLibByName
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")
# then SENDER sends to this port (not tcpflood!)
input(type="imrelp" port="'$PORT_RCVR'" 
		tls="on"
		tls.cacert="'$srcdir/tls-certs/syslog-server-ca.pem'"
		tls.mycert="'$srcdir/tls-certs/syslog-server-crt.pem'"
		tls.myprivkey="'$srcdir/tls-certs/syslog-server-key.pem'"
		tls.authmode="name"
		tls.permittedpeer=["*"])

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

:msg, contains, "msgnum:" action(type="omrelp"
		target="127.0.0.1" port="'$PORT_RCVR'" 
		action.resumeinterval="10"
		action.resumeretrycount="-1"
		queue.type="disk"
		queue.maxdiskspace="100000000"
		queue.filename="relp-queue"
		queue.size="100000"
		queue.discardmark="90000"
		queue.discardseverity="0"
		queue.saveonshutdown="on"
		tls="on"
		tls.cacert="'$srcdir/tls-certs/syslog-ca.pem'"
		tls.mycert="'$srcdir/tls-certs/syslog-crt.pem'"
		tls.myprivkey="'$srcdir/tls-certs/syslog-key.pem'"
		tls.authmode="name"
		tls.permittedpeer=["*"])
action(type="omfile" file="'$RSYSLOG_DYNNAME.errmsgs'")
' 2
startup 2

grep "omrelp error: invalid authmode" $RSYSLOG_DYNNAME.errmsgs > /dev/null
ret=$?
if [ $ret -eq 0 ]; then
        echo "SKIP: librelp does not support "certvalid" auth mode"
	# mini-cleanup to not leave dangling processes
	shutdown_immediate 2
	shutdown_immediate
	rm $RSYSLOG_DYNNAME* &> /dev/null
	exit 77
fi

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg 1 1000

# shut down sender
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 1000
exit_test
