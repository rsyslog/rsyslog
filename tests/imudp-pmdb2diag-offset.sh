#!/bin/bash
# addd 2019-09-24 by alorbach, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")

module(load="../contrib/pmdb2diag/.libs/pmdb2diag")
$RulesetParser db2.diag
$RulesetParser rsyslog.rfc5424
$RulesetParser rsyslog.rfc3164

input(type="imudp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
# Send one large (>100 bytes) raw message
tcpflood -p'$TCPFLOOD_PORT'  -Tudp -b1 -W1 -M"12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890" -m1
tcpflood -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Tudp -b10 -W1 

shutdown_when_empty
wait_shutdown
seq_check
exit_test
