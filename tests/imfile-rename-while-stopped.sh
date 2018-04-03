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

. $srcdir/diag.sh startup imfile-wildcards-simple.conf
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!

# Move to another filename
mv rsyslog.input.1.log rsyslog.input.2.log

# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> rsyslog.input.2.log
ls -li rsyslog.input*
echo ls test-spool:
ls -l test-spool

. $srcdir/diag.sh startup imfile-wildcards-simple.conf
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!

. $srcdir/diag.sh seq-check 0 $TESTMESSAGESFULL
wc rsyslog.out.log
. $srcdir/diag.sh exit
