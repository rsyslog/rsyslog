#!/bin/bash
# Test for the $ActionExecOnlyOnceEveryInterval directive.
# We inject a couple of messages quickly during the interval,
# then wait until the interval expires, then quickly inject
# another set. After that, it is checked if exactly two messages
# have arrived.
# The once interval must be set to 3 seconds in the config file.
# added 2009-11-12 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")
$ActionExecOnlyOnceEveryInterval 3
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
tcpflood -m10 -i1
# now wait until the interval definitely expires (at least we hope so...)
printf 'wainting for interval to expire...\n'
sleep 5
# and inject another couple of messages
tcpflood -m10 -i100
shutdown_when_empty
wait_shutdown

export EXPECTED="00000001
00000100"
cmp_exact
exit_test
