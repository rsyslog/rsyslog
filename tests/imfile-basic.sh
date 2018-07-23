#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
echo [imfile-basic.sh]
. $srcdir/diag.sh init
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen -m 50000 > rsyslog.input
ls -l rsyslog.input
startup imfile-basic.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 49999
exit_test
