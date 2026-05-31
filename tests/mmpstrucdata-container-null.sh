#!/bin/bash
# Verify mmpstrucdata's custom container name and no-SD null handling.
# The oracle is rsyslog's configured omfile output after shutdown: one
# RFC5424 message with structured data must populate the requested container,
# and one RFC5424 message with "-" structured data must create that same
# container with a JSON null value.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")

template(name="outfmt" type="string" string="%$!structured-data%\n")

action(type="mmpstrucdata" jsonRoot="$!structured-data" container="custom-sd")
if $msg contains "MMPSTRUCDATA" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid [test@32473 key="value"] MMPSTRUCDATA with sd'
injectmsg literal '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid - MMPSTRUCDATA without sd'
shutdown_when_empty
wait_shutdown
export EXPECTED='{ "custom-sd": { "test@32473": { "key": "value" } } }
{ "custom-sd": null }'
cmp_exact
exit_test
