#!/bin/bash
# added 2015-09-29 by singh.janmejay
# test re_match rscript-fn, using single quotes for the match
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="*Matched*\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if (re_match($msg, '"'"'.* ([0-9]+)$'"'"')) then {
	 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
tcpflood -m 1 -I $srcdir/testsuites/date_time_msg
shutdown_when_empty
wait_shutdown
content_check "*Matched*"
exit_test
