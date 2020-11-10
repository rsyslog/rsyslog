#!/bin/bash
# test many concurrent tcp connections
# addd 2016-02-23 by RGerhards, released under ASL 2.0
# requires faketime
echo \[now-utc\]: test \$NOW-UTC
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

set $.tnow = $$now-unixtimestamp + 1;

template(name="outfmt" type="string"
	 string="%$now-unixtimestamp%,%$.tnow%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'

. $srcdir/faketime_common.sh

export TZ=TEST-02:00

FAKETIME='2016-01-01 01:00:00' startup
# what we send actually is irrelevant, as we just use system properties.
# but we need to send one message in order to gain output!
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED="1451602800,1451602801"
cmp_exact
exit_test
