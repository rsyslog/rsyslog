#!/bin/bash
# addd 2018-10-09 by Alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '

$ModLoad ../plugins/imrelp/.libs/imrelp # Old Style module loading
$inputrelpserverrun '$TCPFLOOD_PORT' 

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -Trelp-plain -p'$TCPFLOOD_PORT' -m10000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 9999
exit_test
