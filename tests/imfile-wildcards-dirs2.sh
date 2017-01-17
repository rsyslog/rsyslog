#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
echo [imfile-wildcards-dirs2.sh]

export IMFILEINPUTFILES="10"
export IMFILEINPUTFILESSTEPS="5"
export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))

. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
. $srcdir/diag.sh startup imfile-wildcards-dirs.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1

for j in `seq 1 $IMFILEINPUTFILESSTEPS`;
do
	echo "Loop Num $j"

	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		mkdir rsyslog.input.dir$i
		./inputfilegen -m 1 > rsyslog.input.dir$i/file.logfile
	done
	ls -d rsyslog.input.*
	
	# Delete all but first!
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		rm -r rsyslog.input.dir$i
	done
done

# sleep a little to give rsyslog a chance for processing
sleep 1

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILESALL
. $srcdir/diag.sh exit
