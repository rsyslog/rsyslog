#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imhttp/.libs/imhttp"
       liboptions=[ "listening_ports='$IMHTTP_PORT'" ] )
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
NUMMESSAGES=50
for (( i=1; i<=NUMMESSAGES; i++ ))
do
  curl -si -H Content-Type:application/json http://localhost:$IMHTTP_PORT/postrequest -d $'{"foo":"bar","bar":"foo"}\n{"one":"two","three":"four"}'
done
wait_queueempty
shutdown_when_empty
echo "file name: $RSYSLOG_OUT_LOG"
content_count_check '{"foo":"bar","bar":"foo"}' $NUMMESSAGES
content_count_check '{"one":"two","three":"four"}' $NUMMESSAGES
exit_test
