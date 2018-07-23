#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi
grep "\.el6\." <<< $(uname -a)
if [ "$?" == "0" ]; then
	echo "CentOS 6 detected, adding valgrind suppressions"
	export RS_TEST_VALGRIND_EXTRA_OPTS="--suppressions=${srcdir}/imfile-basic-vgthread.supp"
fi


echo [imfile-basic.sh]
. $srcdir/diag.sh init
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m 50000 > rsyslog.input
ls -l rsyslog.input
startup_vgthread imfile-basic.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check 0 49999
exit_test
