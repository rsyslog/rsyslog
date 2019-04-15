#!/bin/bash
# addd 2019-04-12 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20 # should be sufficient to stress DNS cache
generate_conf
add_conf '
global(reverselookup.cache.default.ttl="0")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -m$NUMMESSAGES -c10 # run on 10 connections --> 10 dns cache calls
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check
exit_test
