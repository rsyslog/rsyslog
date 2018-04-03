#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="10"
echo [imfile-wildcards-dirs.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
. $srcdir/diag.sh startup imfile-wildcards-dirs.conf

for i in `seq 1 $IMFILEINPUTFILES`;
do
	mkdir rsyslog.input.dir$i
	./inputfilegen -m 1 > rsyslog.input.dir$i/file.logfile
done
ls -d rsyslog.input.*

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILES
. $srcdir/diag.sh exit
