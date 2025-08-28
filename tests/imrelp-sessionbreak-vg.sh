#!/bin/bash
# added 2020-04-10 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000000 
export USE_VALGRIND="YES"
# TODO remote leak check skip and fix memory leaks caused by session break
export RS_TESTBENCH_LEAK_CHECK=no

mkdir $RSYSLOG_DYNNAME.workdir
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")

global(
    workDirectory="'$RSYSLOG_DYNNAME.workdir'"
    maxMessageSize="256k"
)
main_queue(queue.type="Direct")

$LocalHostName test
$AbortOnUncleanConfig on
$PreserveFQDN on

input(	type="imrelp" 
	name="imrelp" 
	port="'$TCPFLOOD_PORT'" 
	ruleset="spool" 
	MaxDataSize="256k"
	KeepAlive="on"
)
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="spool" queue.type="direct") {
	if $msg contains "msgnum:" then {
		action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
	}
}
'
startup
# How many tcpfloods we run at the same time
for ((i=1;i<=5;i++)); do 
        # How many times tcpflood runs in each threads
	./tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m$NUMMESSAGES -s &
	tcpflood_pid=$!

	echo "started tcpflood instance $i (PID $tcpflood_pid)"

	# Give it time to actually connect
	./msleep 500;

	kill -9 $tcpflood_pid # >/dev/null 2>&1;
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
