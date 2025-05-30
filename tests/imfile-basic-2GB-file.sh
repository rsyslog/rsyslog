#!/bin/bash
# written 2018-11-09 by Rainer Gerhards
# this test checks that 2GiB (31 bit) file size region is handled correctly
# it first generates a file that is 2GiB-64 bytes, processes it, and then
# adds a couple of messages to get it over 2GiB.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export TB_TEST_MAX_RUNTIME=3600 # test is very slow as it works on large files
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="file:")

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
touch $RSYSLOG_DYNNAME.input
startup

# initial file: 2GiB - 1 message (54 byte)
./inputfilegen -s 2147483584 -d47 -M $RSYSLOG_DYNNAME.msgcnt > $RSYSLOG_DYNNAME.input
ls -lh $RSYSLOG_DYNNAME.input
export NUMMESSAGES="$(cat $RSYSLOG_DYNNAME.msgcnt)"

wait_file_lines --delay 2500 --abort-on-oversize "$RSYSLOG_OUT_LOG" $NUMMESSAGES 3000

# add one message --> exactly 2GB
./inputfilegen -m1 -d47 -i$NUMMESSAGES>> $RSYSLOG_DYNNAME.input
ls -lh $RSYSLOG_DYNNAME.input
(( NUMMESSAGES++ ))
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" $NUMMESSAGES

# add one more message --> now we go over 2GB
./inputfilegen -m1 -d47 -i$NUMMESSAGES>> $RSYSLOG_DYNNAME.input
ls -lh $RSYSLOG_DYNNAME.input
(( NUMMESSAGES++ ))
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" $NUMMESSAGES

# add even more messages
./inputfilegen -m10000 -d47 -i$NUMMESSAGES>> $RSYSLOG_DYNNAME.input
ls -lh $RSYSLOG_DYNNAME.input
NUMMESSAGES=$(( NUMMESSAGES + 10000 ))
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" $NUMMESSAGES

shutdown_when_empty
wait_shutdown
seq_check 0 $(( NUMMESSAGES - 1))
exit_test
