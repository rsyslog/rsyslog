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

generate_conf
add_conf '
module(load="../contrib/omrabbitmq/.libs/omrabbitmq")
template(name="rkTpl" type="string" string="%syslogtag%.%syslogfacility-text%.%syslogpriority-text%")
# rfc5424 without Timestamp : unable to manage
template(name="bodyTpl" type="string" string="<%PRI%>1 server %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg:2:$%\n")
ruleset(name="rmq") {
  action(type="omrabbitmq" host="localhost" port="'$PORT_AMQP1'" exchange="in"
         user="mtr" password="mtr" expiration="5000"
         body_template="bodyTpl" content_type="rfc5424"
         virtual_host="/metrologie" routing_key_template="rkTpl"
         populate_properties="on" delivery_mode="transient"
        )
}
if $msg contains "msgrmq" then {
	call rmq
}
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal "<167>Mar  1 01:00:00 172.20.245.8 tag msgrmq"
shutdown_when_empty
wait_shutdown
export EXPECTED=$(printf 'Exchange:in, routing-key:tag.local4.debug, content-type:rfc5424, facility:local4, severity:debug, hostname:172.20.245.8, fromhost:127.0.0.1, delivery-mode:transient, expiration:5000, timestamp:OK, app-id:tag, msg:<167>1 server tag - - -  msgrmq')
echo $EXPECTED | cmp - $RSYSLOG_DYNNAME.amqp.log
if [ ! $? -eq 0 ]; then
  echo "Expected:"
  echo $EXPECTED
  echo "invalid response generated, $RSYSLOG_DYNNAME.amqp.log is:"
  cat $RSYSLOG_DYNNAME.amqp.log
  echo "Rsyslog internal output log:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;
content_check "server localhost port"
exit_test
