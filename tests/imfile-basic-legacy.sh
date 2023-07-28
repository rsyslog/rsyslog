#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
NUMMESSAGES=50000
mkdir WorkDirectory  $RSYSLOG_DYNNAME.work
generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.work
$ModLoad ../plugins/imfile/.libs/imfile
$InputFileName ./'$RSYSLOG_DYNNAME'.input
$InputFileTag file:
$InputFileStateFile stat-file1
$InputFileSeverity error
$InputFileFacility local7
$InputFileMaxLinesAtOnce 100000
$InputRunFileMonitor

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m $NUMMESSAGES > $RSYSLOG_DYNNAME.input
startup
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
