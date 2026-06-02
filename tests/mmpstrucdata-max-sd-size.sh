#!/bin/bash
# Verify mmpstrucdata's maxStructuredDataSize guard skips oversized structured
# data. The oracle uses configured omfile output after shutdown for the accepted
# message; stderr/stdout are not used as proof.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")

template(name="outfmt" type="string" string="%$!structured-data!custom-sd!small@32473!ok%\n")

action(type="mmpstrucdata" jsonRoot="$!structured-data" container="custom-sd" maxStructuredDataSize="64")
if $!structured-data!custom-sd!small@32473!ok == "yes" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid [large@32473 blob="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" tail="skip"] MMPSTRUCDATA too large'
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid [small@32473 ok="yes"] MMPSTRUCDATA accepted'
shutdown_when_empty
wait_shutdown
export EXPECTED='yes'
cmp_exact
exit_test
