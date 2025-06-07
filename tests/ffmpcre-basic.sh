#!/bin/bash
# added 2025-06-07 by Codex, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/fmpcre/.libs/fmpcre")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$.hit%\n")
set $.hit = pcre_match($msg, "h.llo");
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

startup
tcpflood -m1 -M "\"<13>Jan 1 00:00:00 host tag: hello\""
tcpflood -m1 -M "\"<13>Jan 1 00:00:00 host tag: hallo\""
shutdown_when_empty
wait_shutdown
echo '1
1' | cmp - $RSYSLOG_OUT_LOG
if [ $? -ne 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi

exit_test

