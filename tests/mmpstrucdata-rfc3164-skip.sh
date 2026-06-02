#!/bin/bash
# Verify mmpstrucdata still ignores legacy messages without RFC5424 structured
# data. The oracle is absence of configured omfile output after shutdown; if the
# module created the no-SD JSON null for RFC3164 input, the output file would be
# created.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")

template(name="outfmt" type="string" string="%$!structured-data%\n")

action(type="mmpstrucdata" jsonRoot="$!structured-data" container="custom-sd")
if $!structured-data != "" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal '<85>May 22 08:00:00 host app: MMPSTRUCDATA legacy without sd'
shutdown_when_empty
wait_shutdown
check_file_not_exists "$RSYSLOG_OUT_LOG"
exit_test
