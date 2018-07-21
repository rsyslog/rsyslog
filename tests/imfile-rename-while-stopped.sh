#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export TESTMESSAGES=10000
export RETRIES=10
export TESTMESSAGESFULL=19999
echo [imfile-rename.sh]
. $srcdir/diag.sh check-inotify-only
. $srcdir/diag.sh init

# generate input file first. 
./inputfilegen -m $TESTMESSAGES > rsyslog.input.1.log
ls -li rsyslog.input*

startup imfile-wildcards-simple.conf
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

# Move to another filename
mv rsyslog.input.1.log rsyslog.input.2.log

# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> rsyslog.input.2.log
ls -li rsyslog.input*
echo ls test-spool:
ls -l test-spool

startup imfile-wildcards-simple.conf
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

seq_check 0 $TESTMESSAGESFULL
wc rsyslog.out.log
exit_test
