#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD" "This test currently does not work on FreeBSD."
export NUMMESSAGES=50000
if grep -q 'release 6\.' <<< "$(cat /etc/redhat-release 2>/dev/null)"; then
	echo "RHEL/CentOS 6 detected, adding valgrind suppressions"
	export RS_TEST_VALGRIND_EXTRA_OPTS="--suppressions=${srcdir}/imfile-basic-vgthread.supp"
fi

exit
generate_conf
add_conf '
$ModLoad ../plugins/imfile/.libs/imfile
$InputFileName ./'$RSYSLOG_DYNNAME'.input
$InputFileTag file:
$InputFileStateFile stat-file1
$InputFileSeverity error
$InputFileFacility local7
$InputFileMaxLinesAtOnce 100000
$InputRunFileMonitor

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" ./'$RSYSLOG_OUT_LOG';outfmt
'

# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m $NUMMESSAGES > $RSYSLOG_DYNNAME.input
ls -l $RSYSLOG_DYNNAME.input

startup_vgthread
wait_file_lines
shutdown_when_empty
wait_shutdown_vg
check_exit_vg

seq_check
exit_test
