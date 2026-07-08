#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
# Verify UDP tag parsing when the sender tag has no traditional length limit.
# The imudp listener owns an OS-assigned port before tcpflood sends, preventing
# parallel UDP tests from sharing a preselected port; the ordered tag output is
# the pass/fail oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
PORT_RCVR_FILE="$RSYSLOG_DYNNAME.imudp.port"
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$PORT_RCVR_FILE'")

template(name="outfmt" type="string" string="+%syslogtag%+\n")

:pri, contains, "167" action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
				   template="outfmt")


'
startup
assign_tcpflood_port "$PORT_RCVR_FILE"
tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 TAG: Rest of message...\""
tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 0 Rest of message...\""
tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901 Rest of message...\""
tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 01234567890123456789012345678901-toolong Rest of message...\""
shutdown_when_empty
wait_shutdown

echo '+TAG:+
+0+
+01234567890123456789012345678901+
+01234567890123456789012345678901-toolong+' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
