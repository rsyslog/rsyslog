#!/bin/bash
# add 2019-09-03 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
cmd="./miniamqpsrvr -f $RSYSLOG_DYNNAME.amqp.log -d"
echo $cmd
eval $cmd > $RSYSLOG_DYNNAME.source
if [ ! $? -eq 0 ]; then
  exit 77
fi

. $RSYSLOG_DYNNAME.source
export OMRABBITMQ_TEST=1

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/omrabbitmq/.libs/omrabbitmq")
ruleset(name="rmq") {
  action(type="omrabbitmq" host="localhost" port="'$PORT_AMQP1'"
         user="mtr" password="mtr" exchange="in" expiration="5000"
         virtual_host="/metrologie" routing_key="myrouting"
        )
}
if $msg contains "msgrmq" then {
	call rmq
}
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal "<167>Mar  1 01:00:00 192.0.2.8 tag msgrmq"
shutdown_when_empty
wait_shutdown
expected=$(printf 'Exchange:in, routing-key:myrouting, content-type:application/json, delivery-mode:transient, expiration:5000, msg:{\"message\":\" msgrmq\",\"fromhost\":\"192.0.2.8\",\"facility\":\"local4\",\"priority\":\"debug\",\"timereported\":.*}')
grep -E "${expected}" $RSYSLOG_DYNNAME.amqp.log > /dev/null 2>&1
if [ ! $? -eq 0 ]; then
  echo "Expected:"
  echo ${expected}
  echo "invalid response generated, $RSYSLOG_DYNNAME.amqp.log is:"
  cat $RSYSLOG_DYNNAME.amqp.log
  echo "Rsyslog internal output log:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi
content_check "server localhost port"
exit_test
