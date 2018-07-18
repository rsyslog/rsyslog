#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. $srcdir/diag.sh init
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m 50000 > rsyslog.input
ls -l rsyslog.input
. $srcdir/diag.sh startup imfile-basic.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh seq-check 0 49999
. $srcdir/diag.sh exit
