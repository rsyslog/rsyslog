#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
echo [imfile-wildcards.sh]

export IMFILEINPUTFILES="10"

. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

imfilebefore="rsyslog.input.1"
./inputfilegen -m 1 > $imfilebefore

# Start rsyslog now before adding more files
. $srcdir/diag.sh startup imfile-wildcards.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1

for i in `seq 2 $IMFILEINPUTFILES`;
do
	cp $imfilebefore rsyslog.input.$i
	imfilebefore="rsyslog.input.$i"
done
ls -l rsyslog.input.*

# sleep a little to give rsyslog a chance for processing
sleep 1

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILES
. $srcdir/diag.sh exit
