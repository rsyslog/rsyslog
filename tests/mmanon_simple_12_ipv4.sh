#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.bits="12" ipv4.mode="simple")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: 1.1.1.8
<129>Mar 10 01:00:00 172.20.245.8 tag: 0.0.0.0
<129>Mar 10 01:00:00 172.20.245.8 tag: 172.0.234.255
<129>Mar 10 01:00:00 172.20.245.8 tag: 111.1.1.8.\""

shutdown_when_empty
wait_shutdown
echo ' 1.1.x.x
 0.0.x.x
 172.0.xxx.xxx
 111.1.x.x.' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

grep 'invalid number of ipv4 bits in simple mode, corrected to 16' ${RSYSLOG2_OUT_LOG} > /dev/null
if [ $? -ne 0 ]; then
  echo "invalid response generated, ${RSYSLOG2_OUT_LOG} is:"
  cat ${RSYSLOG2_OUT_LOG}
  error_exit  1
fi;

exit_test
