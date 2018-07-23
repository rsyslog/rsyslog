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
ls -l rsyslog.input*

startup imfile-wildcards-simple.conf

# sleep a little to give rsyslog a chance to begin processing

. $srcdir/diag.sh wait-file-lines rsyslog.out.log $TESTMESSAGES $RETRIES

# Move to another filename
mv rsyslog.input.1.log rsyslog.input.2.log

./msleep 500
# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> rsyslog.input.2.log
ls -l rsyslog.input*
echo ls test-spool:
ls -l test-spool
./msleep 500

let msgcount="2* $TESTMESSAGES"
. $srcdir/diag.sh wait-file-lines rsyslog.out.log $msgcount $RETRIES

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 $TESTMESSAGESFULL
exit_test
