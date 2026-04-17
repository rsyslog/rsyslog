#!/bin/bash
# added 2020-04-10 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
export DTLS_SESSIONBREAK_ROUNDS=${DTLS_SESSIONBREAK_ROUNDS:-16}
export USE_VALGRIND="yes"
# TODO remote leak check skip and fix memory leaks caused by session break
export RS_TESTBENCH_LEAK_CHECK=no
export PORT_RCVR="$(get_free_port)"

mkdir $RSYSLOG_DYNNAME.workdir
generate_conf
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem"
	workDirectory="'$RSYSLOG_DYNNAME.workdir'"
	maxMessageSize="256k")
main_queue(queue.type="Direct")

$LocalHostName test
$AbortOnUncleanConfig on
$PreserveFQDN on

module(	load="../plugins/imdtls/.libs/imdtls" 
	tls.AuthMode="x509/certvalid")

input(	type="imdtls"
	port="'$PORT_RCVR'"
	ruleset="spool"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="spool" queue.type="direct") {
	if $msg contains "msgnum:" then {
		action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
	}
}
'
startup
# Stress repeated DTLS client churn to exercise session cleanup and fd reuse.
for ((i=1;i<=DTLS_SESSIONBREAK_ROUNDS;i++)); do
	./tcpflood -Tdtls -p$PORT_RCVR -m$NUMMESSAGES -W1000 -d102400 -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem -s &
	tcpflood_pid=$!

	echo "started tcpflood instance $i (PID $tcpflood_pid)"

	# Give it time to actually connect
	./msleep 1000;

	kill -9 $tcpflood_pid >/dev/null 2>&1 || true
	wait $tcpflood_pid >/dev/null 2>&1 || true
	echo "killed tcpflood instance $i (PID $tcpflood_pid)"
done;

wait_queueempty

netstatresult=$(netstat --all --program 2>&1 | grep "ESTABLISHED" | grep $(cat $RSYSLOG_PIDBASE.pid) | grep $TCPFLOOD_PORT)
openfd=$(ls -l "/proc/$(cat $RSYSLOG_PIDBASE$1.pid)/fd" | wc -l)

shutdown_when_empty
wait_shutdown

if [[ "$netstatresult" == "" ]]
then
	echo "OK!"
else
	echo "STILL OPENED Connections: "
	echo $netstatresult
	echo "Open files at the end: "
	echo $openfd
	error_exit 1 
fi

exit_test
