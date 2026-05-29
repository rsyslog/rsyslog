#!/usr/bin/env bash
# Regression test for GitHub issue #4024. A missing mmdbfile must suspend the
# mmdblookup action with a clear diagnostic instead of leaving a half-created
# worker instance that can later crash while rsyslog shuts down.
. ${srcdir:=.}/diag.sh init
require_plugin mmdblookup

generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!iplocation%\n")

module(load="../plugins/mmdblookup/.libs/mmdblookup")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmnormalize" rulebase="'$srcdir'/mmdb.rb")
	action(type="mmdblookup"
	       mmdbfile="'$RSYSLOG_DYNNAME'.missing.mmdb"
	       key="$!ip"
	       fields="city"
	       action.resumeRetryCount="0")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'

startup
tcpflood -m 1 -j "202.106.0.20\ "
shutdown_when_empty
wait_shutdown

check_not_present "Segmentation fault" "${RSYSLOG_DYNNAME}.started"
content_check "maxminddb error: cannot open database file" "${RSYSLOG_DYNNAME}.started"
exit_test
