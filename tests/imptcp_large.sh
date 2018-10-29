#!/bin/bash
# Test imptcp with large messages
# added 2010-08-10 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(maxMessageSize="10k")
template(name="outfmt" type="string" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")

module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="'$TCPFLOOD_PORT'" ruleset="testing")

ruleset(name="testing") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
# send messages of 10.000bytes plus header max, randomized
tcpflood -c5 -m20000 -r -d10000
. $srcdir/diag.sh wait-file-lines $RSYSLOG_OUT_LOG 20000
shutdown_when_empty
wait_shutdown
seq_check 0 19999 -E
exit_test
