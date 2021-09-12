#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
IMHTTP_PORT="$(get_free_port)"
add_conf '
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

#
# note htpasswd file contains entry:
# user1:1234
#
module(load="../contrib/imhttp/.libs/imhttp"
       ports="'$IMHTTP_PORT'")
input(type="imhttp" endpoint="/postrequest"
      ruleset="ruleset"
      addmetadata="on"
      basicauthfile="'$srcdir'/testsuites/htpasswd")
ruleset(name="ruleset") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
# validate invalid passwd
ret=$(curl -v -s -H Content-Type:application/json -H "BAZ: skat" -H "daddle: doodle" -H "fiddle: faddle" \
  --user user1:badpass --write-out '%{http_code}' \
  http://localhost:$IMHTTP_PORT/postrequest -d '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}]')

echo "response: $ret"
if ! ( echo "$ret" | grep "Error 401: Unauthorized" )
then
  echo "ERROR: should not be unauthorized"
  error_exit 1
fi

# validate correct password
curl -s -H Content-Type:application/json -H "BAZ: skat" -H "daddle: doodle" -H "fiddle: faddle" \
  --user user1:1234 --write-out '%{http_code}' \
  http://localhost:$IMHTTP_PORT/postrequest -d '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}]'

wait_queueempty
echo "doing shutdown"
shutdown_when_empty
wait_shutdown
echo "file name: $RSYSLOG_OUT_LOG"
content_check '[{"foo":"bar","bar":"foo"},{"one":"two","three":"four"}] - { baz: skat, daddle: doodle, fiddle: faddle }'
exit_test
