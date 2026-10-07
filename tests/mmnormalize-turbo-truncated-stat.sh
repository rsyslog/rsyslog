#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# A TurboVM result that hits a per-result capacity is missing fields the
# rulebase did match, so mmnormalize declines it and lets the standard parser
# produce the complete set. Two things must hold: the message still carries
# every field, and mmnormalize/turbo.truncated counts the decline so the double
# parse is visible to an operator.
#
# The oracle is the field set, not just the counter: a counter that ticks while
# fields go missing would be worse than no counter at all.
#
# Configure only schedules this test when liblognorm exposes
# ln_fast_result_is_truncated() and impstats is enabled. The fast result's
# field count is uint8_t, so its capacity cannot exceed 255. A 256-field
# rule guarantees truncation even if the dependency raises its current cap.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
require_plugin impstats

# Exceed the maximum possible capacity of the uint8_t field count.
rules=""
i=0
while [ $i -lt 256 ]; do
	rules="$rules%f$i:word% "
	i=$((i + 1))
done

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/impstats/.libs/impstats" interval="1"
	log.file="'$RSYSLOG_DYNNAME'.stats" log.syslog="off" format="cee")
input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="norm")

# f0 is the first field and f255 the last: both must survive the fallback
template(name="ends" type="string" string="%$!f0%-%$!f255%\n")

ruleset(name="norm") {
	action(type="mmnormalize" useRawMsg="on" turbo="on"
		rule="rule=:'"$rules"'")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ends")
}
'
startup
# 256 space-separated words, so every %fN:word% binds
msg=$(awk 'BEGIN{for(i=0;i<256;i++) printf "w%d ", i}')
tcpflood -m1 -M "\"$msg\""
shutdown_when_empty
wait_shutdown

# the standard parser must have supplied the whole field set
export EXPECTED='w0-w255'
cmp_exact "$RSYSLOG_OUT_LOG"

# and the decline must be visible in impstats
content_check --regex '"turbo.truncated": *[1-9]' "$RSYSLOG_DYNNAME.stats"
exit_test
