#!/bin/bash
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(parser.EscapeControlCharacterTab="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset1")

$ErrorMessagesToStderr off

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
	       template="outfmt")
}

'
startup
tcpflood -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: before HT	after HT (do NOT remove TAB!)\""
shutdown_when_empty
wait_shutdown

export EXPECTED=' before HT	after HT (do NOT remove TAB!)'
cmp_exact

exit_test
