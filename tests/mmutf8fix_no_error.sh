#!/bin/bash
# add 2018-10-10 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmutf8fix")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: this is a test message
<129>Mar 10 01:00:00 172.20.245.8 tag: special characters: ??%%,,..
<129>Mar 10 01:00:00 172.20.245.8 tag: numbers: 1234567890\""

shutdown_when_empty
wait_shutdown
echo ' this is a test message
 special characters: ??%%,,..
 numbers: 1234567890' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
