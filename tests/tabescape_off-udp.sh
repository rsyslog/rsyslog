#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
# Verify that disabling tab escaping preserves the literal tab in UDP input.
# Bind a private loopback UDP port before publishing it to the sender so
# parallel tests cannot share a port selected from the TCP port namespace.
# The exact output comparison below is the literal-tab preservation oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.udp_port" ruleset="ruleset1")

$ErrorMessagesToStderr off
$EscapeControlCharacterTab off

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
	       template="outfmt")
}

'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.udp_port"
tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: before HT	after HT (do NOT remove TAB!)\""
shutdown_when_empty
wait_shutdown

echo ' before HT	after HT (do NOT remove TAB!)' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
