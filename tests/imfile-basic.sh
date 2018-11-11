#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" tag="file:")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
# make sure file exists when rsyslog starts up
touch $RSYSLOG_DYNNAME.input
startup
./inputfilegen -m $NUMMESSAGES >> $RSYSLOG_DYNNAME.input
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
