#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
sleep 1
NUMMESSAGES=50
for (( i=1; i<=NUMMESSAGES; i++ ))
do
  curl -si -H Content-Type:application/json http://localhost:$IMHTTP_PORT/postrequest -d '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}]' &
  pids[${i}]=$!
done

# wait for all pids
for pid in ${pids[*]};
do
  wait $pid
done

sleep 2
wait_queueempty
echo "doing shutdown"
shutdown_when_empty
wait_shutdown
echo "file name: $RSYSLOG_OUT_LOG"
content_count_check '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}]' $NUMMESSAGES
exit_test
