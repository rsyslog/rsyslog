#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
template(name="all" type="string" string="%msg% headers: %$!metadata%\n")
template(name="outfmt" type="list") {
  property(name="msg")
  constant(value=" - ")
  constant(value="{ baz: ")
  property(name="$!metadata!httpheaders!baz")
  constant(value=", daddle: ")
  property(name="$!metadata!httpheaders!daddle")
  constant(value=", fiddle: ")
  property(name="$!metadata!httpheaders!fiddle")
  constant(value=" }")
  constant(value=" - ")
  constant(value=" !metadata: ")
  property(name="$!metadata")
  constant(value="\n")
}

module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest" ruleset="ruleset" addmetadata="on")
ruleset(name="ruleset") {
	#action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="all")
}
'
startup
NUMMESSAGES=100
for (( i=1; i<=NUMMESSAGES; i++ ))
do
  curl -si -H Content-Type:application/json -H "BAZ: skat" -H "daddle: doodle" -H "fiddle: faddle" \
    "http://localhost:$IMHTTP_PORT/postrequest?foo=bar;bar=foo&one=3" --data '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}]' &
done
sleep 2
wait_queueempty
echo "doing shutdown"
shutdown_when_empty
wait_shutdown
echo "file name: $RSYSLOG_OUT_LOG"
#content_count_check '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}] - { baz: skat, daddle: doodle, fiddle: faddle }' $NUMMESSAGES
content_count_check '"queryparams": { "foo": "bar", "bar": "foo", "one": "3" }' $NUMMESSAGES
exit_test
