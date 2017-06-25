#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export TESTMESSAGES=10000
export TESTMESSAGESFULL=19999

echo [imfile-rename.sh]

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris does not support inotify."
   exit 77
fi

. $srcdir/diag.sh init

# generate input file first. 
./inputfilegen -m $TESTMESSAGES > rsyslog.input.1.log
ls -l rsyslog.input*

. $srcdir/diag.sh startup imfile-wildcards-simple.conf

# sleep a little to give rsyslog a chance to begin processing
sleep 5

# Move to another filename
mv rsyslog.input.1.log rsyslog.input.2.log

# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> rsyslog.input.2.log
ls -l rsyslog.input*

# sleep a little to give rsyslog a chance to begin processing
sleep 5

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh seq-check 0 $TESTMESSAGESFULL
. $srcdir/diag.sh exit
