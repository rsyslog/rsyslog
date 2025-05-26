#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
generate_conf
# NOTE: do NOT set a working directory!
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" tag="file:")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
else
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.othermsgs")
'
# make sure file exists when rsyslog starts up
touch $RSYSLOG_DYNNAME.input
startup
./inputfilegen -m $NUMMESSAGES >> $RSYSLOG_DYNNAME.input
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
content_check "imfile: no working or state file directory set" $RSYSLOG_DYNNAME.othermsgs
exit_test
