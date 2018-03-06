#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="1"
export IMFILEINPUTFILESSTEPS="5"
#export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))
export IMFILECHECKTIMEOUT="10"
. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

for i in `seq 1 $IMFILEINPUTFILES`;
do
	echo "Make rsyslog.input.dir$i"
	mkdir rsyslog.input.dir$i
		echo created!
done

# Start rsyslog now before adding more files
. $srcdir/diag.sh startup imfile-wildcards-dirs-multi3.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 2

for j in `seq 1 $IMFILEINPUTFILESSTEPS`;
do
	echo "Loop Num $j"
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		echo "Make rsyslog.input.dir$i/dir$j/testdir"
		mkdir rsyslog.input.dir$i/dir$j
		mkdir rsyslog.input.dir$i/dir$j/testdir
		mkdir rsyslog.input.dir$i/dir$j/testdir/subdir$j
		./inputfilegen -m 1 > rsyslog.input.dir$i/dir$j/testdir/subdir$j/file.logfile
		ls -l rsyslog.input.dir$i/dir$j/testdir/subdir$j/file.logfile
	done
	ls -d rsyslog.input.*

	# Check correct amount of input files each time
	let IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $j))
	. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILESALL $IMFILECHECKTIMEOUT

	# Delete all but first!
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		rm -rf rsyslog.input.dir$i/dir$j/testdir/subdir$j/file.logfile
		rm -rf rsyslog.input.dir$i/dir$j
	done
done

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh exit
