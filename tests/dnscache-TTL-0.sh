#!/bin/bash
# add 2019-04-12 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=20 # should be sufficient to stress DNS cache
generate_conf
add_conf '
global(reverselookup.cache.ttl.default="0"
       reverselookup.cache.ttl.enable="on")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m$NUMMESSAGES -c10 # run on 10 connections --> 10 dns cache calls
shutdown_when_empty
wait_shutdown
seq_check
exit_test
