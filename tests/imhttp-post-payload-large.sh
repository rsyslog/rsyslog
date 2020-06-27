#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
#template(name="outfmt" type="string" string="%msg%\n")
template(name="outfmt" type="string" string="%rawmsg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
NUMMESSAGES=25
DATA_SOURCE=$srcdir/testsuites/imhttp-large-data.txt
for (( i=1; i<=NUMMESSAGES; i++ ))
do
  curl -si -X POST -H Content-Type:application/json http://localhost:$IMHTTP_PORT/postrequest --data-binary @$DATA_SOURCE &
  pids[${i}]=$!
done

# wait for all pids
for pid in ${pids[*]};
do
  wait $pid
#  echo "$pid cleaned up"
done

sleep 2
wait_queueempty
echo "doing shutdown"
shutdown_when_empty
wait_shutdown

# reference file for comparison
TMPFILE=${RSYSLOG_DYNNAME}.tmp
for (( i=1; i <= NUMMESSAGES; i++ ))
do
  cat $DATA_SOURCE >> $TMPFILE
done

if diff -q $TMPFILE $RSYSLOG_OUT_LOG;
then
  echo "files match!"
  exit_test
else
  error_exit 1
fi
