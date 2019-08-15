#!/bin/bash
# add 2016-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/mmtaghostname/.libs/mmtaghostname")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset")
template(name="test" type="string" string="tag: %syslogtag%, server: %hostname%, msg: %msg%\n")
ruleset(name="ruleset") {
	action(type="mmtaghostname" tag="source-imtcp")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="test")
}
'
startup
tcpflood -m1 -M "\"<189>1 2019-03-03T16:09:56.185+00:00 server app 123.4 msgid - %SYS-5-CONFIG_I: Configured from console by adminsepp on vty0 (10.23.214.226)\""
shutdown_when_empty
wait_shutdown
echo 'tag: source-imtcp, server: server, msg: %SYS-5-CONFIG_I: Configured from console by adminsepp on vty0 (10.23.214.226)' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test

