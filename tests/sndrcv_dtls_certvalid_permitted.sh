#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
printf 'using TLS driver: %s\n' ${RS_TLS_DRIVER:=gtls}
export NUMMESSAGES=1000
export NUMMESSAGESFULL=$NUMMESSAGES
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines

# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
export PORT_RCVR="$(get_free_port)"

add_conf '
global(
#	debug.whitelist="on"
#	debug.files=["imdtls.c", "modules.c", "errmsg.c", "action.c"]
)

module(	load="../plugins/imdtls/.libs/imdtls" )
input(	type="imdtls"
	port="'$PORT_RCVR'"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem"
	tls.authmode="name"
	tls.permittedpeer="rsyslog"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup

export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
#valgrind="valgrind"
generate_conf 2
add_conf '

global(
#	debug.whitelist="on"
#	debug.files=["omdtls.c", "modules.c", "errmsg.c", "action.c"]
)

# impstats in order to gain insight into error cases
module(load="../plugins/impstats/.libs/impstats"
	log.file="'$RSYSLOG_DYNNAME.pstats'"
	interval="1" log.syslog="off")
$imdiagInjectDelayMode full

# Load modules
module(	load="../plugins/omdtls/.libs/omdtls" )

local4.* {
	action(	name="omdtls"
	type="omdtls"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem"
	action.resumeInterval="1"
	action.resumeRetryCount="2"
	queue.type="FixedArray"
	queue.size="10000"
	queue.dequeueBatchSize="1"
	queue.minDequeueBatchSize.timeout="1000" # 1 sec
	queue.timeoutWorkerthreadShutdown="1000" # 1 sec
	queue.timeoutshutdown="1000"
	# Slow down, do not lose UDP messages
	queue.dequeueSlowDown="1000"
	)

	stop
}

action( type="omfile" file="'$RSYSLOG_DYNNAME.othermsg'")
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
