#!/bin/bash
# addd 2016-03-28 by RGerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="-%$!non!existing!var:109:116:%-\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
echo "--" | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  echo "expected was:"
  echo "--"
  exit 1
fi;
exit_test
