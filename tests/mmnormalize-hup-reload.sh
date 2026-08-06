#!/bin/bash
# Verify that HUP safely replaces the shared non-Turbo liblognorm context and
# that messages after the acknowledged HUP use the new rulebase. The two
# synchronized output records are the oracle: the first must use the original
# rule and the second the replacement, with clean daemon shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

printf '%s\n' 'rule=:old:%value:word%' > "$RSYSLOG_DYNNAME.rulebase"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="normalized" type="string" string="%$!value%\n")

if $rawmsg contains "old:" or $rawmsg contains "new:" then {
	action(type="mmnormalize" useRawMsg="on"
		rulebase="'$RSYSLOG_DYNNAME'.rulebase")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="normalized"
		flushOnTXEnd="on")
}
'

startup
tcpflood -m1 -M '"old:first"'
wait_file_lines "$RSYSLOG_OUT_LOG" 1 30

printf '%s\n' 'rule=:new:%value:word%' > "$RSYSLOG_DYNNAME.rulebase"
issue_HUP

tcpflood -m1 -M '"new:second"'
shutdown_when_empty
wait_shutdown

export EXPECTED=$'first\nsecond'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
