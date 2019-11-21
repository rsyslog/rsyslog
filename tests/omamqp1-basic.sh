#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/omamqp1-common.sh

AMQP_URL=${AMQP_URL:-"localhost:5672"}
NUMMESSAGES=10240

generate_conf
if [ "${USE_VALGRIND:-}" = YES ] ; then
	add_conf '
global(debug.unloadModules="off")
'
fi
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/impstats/.libs/impstats" interval="1"
	   log.file="'"$RSYSLOG_DYNNAME.spool"'/stats.log" log.syslog="off" format="cee")
module(load="../contrib/omamqp1/.libs/omamqp1")

if $msg contains "msgnum:" then
	action(type="omamqp1"
	       host="'"$AMQP_URL"'"
	       target="amq.rsyslogtest")
'
qdrouterd > $RSYSLOG_DYNNAME.spool/qdrouterd.log 2>&1 & qdrouterdpid=$!
sleep 5 # give qdrouterd a chance to start up and listen
# have to start reader before writer - like a pipe - for the client, NUMMESSAGES could
# be much less than the number of records if batching is used - for the client, it means
# batches, not records - but there can't be more batches than records
amqp_simple_recv $AMQP_URL amq.rsyslogtest $NUMMESSAGES > $RSYSLOG_DYNNAME.spool/amqp_simple_recv.out 2>&1 &
#export RSYSLOG_DEBUG=debug
#export RSYSLOG_DEBUGLOG=/tmp/rsyslog.debug.log
startup
if [ -n "${USE_GDB:-}" ] ; then
    echo attach gdb here
    sleep 54321 || :
fi
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown

timeout=60
for ii in $( seq 1 $timeout ) ; do
	if grep -q "msgnum:00*$(( NUMMESSAGES - 1 ))" $RSYSLOG_DYNNAME.spool/amqp_simple_recv.out ; then
		break
	fi
	sleep 1
done
kill $qdrouterdpid > /dev/null 2>&1 || kill -9 $qdrouterdpid > /dev/null 2>&1 || :
# you would think there would be a better way to do this . . .
kill $( pgrep -f 'python.*simple_recv.py' )
if [ $ii = $timeout ] ; then
	echo ERROR: amqp_simple_recv did not receive all $NUMMESSAGES messages in $timeout seconds
	error_exit 1
fi

$PYTHON -c 'import sys
inp = file(sys.argv[1],"r").read()
last = 0
idx = inp.find("msgnum:",last)
while idx > -1:
    msgstr = inp[(idx+7):(idx+15)]
    print msgstr
    last = idx+16
    idx = inp.find("msgnum:",last)
' $RSYSLOG_DYNNAME.spool/amqp_simple_recv.out > $RSYSLOG_OUT_LOG
seq_check
