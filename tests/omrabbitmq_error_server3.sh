#!/bin/bash
# add 2019-09-03 by Philippe Duveau, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
cmd="./miniamqpsrvr -b 2 -f $RSYSLOG_DYNNAME.amqp.log -w 500 -d"
echo $cmd
eval $cmd > $RSYSLOG_DYNNAME.source
if [ ! $? -eq 0 ]; then
  exit 77
fi

. $RSYSLOG_DYNNAME.source
export OMRABBITMQ_TEST=2000

generate_conf
add_conf '
global(localhostname="server")
module(load="../contrib/omrabbitmq/.libs/omrabbitmq")
template(name="rkTpl" type="string" string="%syslogtag%.%syslogfacility-text%.%syslogpriority-text%")
# rfc5424 without Timestamp : unable to manage
template(name="bodyTpl" type="string" string="<%PRI%>1 %HOSTNAME% %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg:2:$%\n")
ruleset(name="rmq") {
  action(type="omrabbitmq" host="localhost:'$PORT_AMQP1' localhost:'$PORT_AMQP2'" port="5672"
         user="mtr" password="mtr" exchange="in" expiration="5000"
         body_template="" content_type="rfc5424"
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
injectmsg literal "<167>Mar  1 01:00:00 192.0.2.8 tag msgrmq"
shutdown_when_empty
wait_shutdown
export EXPECTED='Exchange:in, routing-key:tag.local4.debug, content-type:plain/text, facility:local4, severity:debug, hostname:192.0.2.8, fromhost:127.0.0.1, delivery-mode:transient, expiration:5000, timestamp:OK, app-id:tag, msg:<167>Mar  1 01:00:00 192.0.2.8 tag msgrmq'
cmp_exact $RSYSLOG_DYNNAME.amqp.log
content_check "Connection closed : reconnect"
exit_test
