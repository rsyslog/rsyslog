#!/bin/bash
# This test tests impstats omfwd counters in UPD mode
# added 2021-02-11 by rgerhards. Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
export STATSFILE="$RSYSLOG_DYNNAME.stats"
add_conf '
$MainMsgQueueTimeoutShutdown 10000


module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'"
	interval="1" ruleset="stats")

ruleset(name="stats") {
	stop # nothing to do here
}

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
module(load="builtin:omfwd" template="outfmt")

if $msg contains "msgnum:" then
	action(type="omfwd" target="127.0.0.1" port="'$TCPFLOOD_PORT'" protocol="udp")
'
# note: there must be no actual data - it's fine for this test if all data is lost

# now do the usual run
startup
# 10000 messages should be enough
injectmsg 0 10000
shutdown_when_empty
wait_shutdown

# check pstats - that's our prime test target
content_check --regex "UDP-.*origin=omfwd .*bytes.sent=[1-9][0-9][0-9]" "$STATSFILE"
exit_test
