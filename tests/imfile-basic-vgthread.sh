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
	export RS_TEST_VALGRIND_EXTRA_OPTS="--suppressions=imfile-basic-vgthread.supp"
fi


echo [imfile-basic.sh]
. $srcdir/diag.sh init
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m 50000 > rsyslog.input
ls -l rsyslog.input
. $srcdir/diag.sh startup-vgthread imfile-basic.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 49999
. $srcdir/diag.sh exit
