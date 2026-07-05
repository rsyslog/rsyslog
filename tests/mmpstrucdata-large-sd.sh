#!/bin/bash
# Verify mmpstrucdata handles RFC5424 structured data larger than 65535 bytes.
# The oracle is a small field after the large value. If the cached structured
# data length wraps or the parser stops early, that trailing field is not written
# to the configured omfile destination.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(maxMessageSize="100k")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")

input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!structured-data!custom-sd!large@32473!tail%\n")

action(type="mmpstrucdata" jsonRoot="$!structured-data" container="custom-sd")
if $msg contains "MMPSTRUCDATA" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
{
    printf '%s' '<85>1 2026-05-22T08:00:00.000+00:00 host app proc msgid [large@32473 blob="'
    awk 'BEGIN { for (i = 0; i < 70000; ++i) printf "x" }'
    printf '%s\n' '" tail="ok"] MMPSTRUCDATA large sd'
} >"$RSYSLOG_DYNNAME.input"
tcpflood -m1 -I "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown
export EXPECTED='ok'
cmp_exact
exit_test
