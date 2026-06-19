#!/bin/bash
# Test global(parser.parseHostnameAndTag="off") for the RFC3164 parser.
# The oracle is the configured omfile output after synchronized shutdown:
# syslogtag must remain empty and the hostname/tag text must remain in %msg%.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(parser.parseHostnameAndTag="off")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="tag=[%syslogtag%] msg=[%msg%]\n")

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

startup
tcpflood -m1 -M "\"<167>Mar 27 19:06:53 source_server sshd[123]: payload\""
shutdown_when_empty
wait_shutdown

content_check "tag=[] msg=[source_server sshd[123]: payload]"
exit_test
