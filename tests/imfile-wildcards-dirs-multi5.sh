#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="8" #"8"
export IMFILEINPUTFILESSTEPS="5" #"5"
#export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))
export IMFILECHECKTIMEOUT="20"
. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global( debug.whitelist="on"
	debug.files=["imfile.c"])

module(load="../plugins/imfile/.libs/imfile" mode="inotify" pollingInterval="1")

input(type="imfile" File="./rsyslog.input.dir?/*/*.logfile"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")
input(type="imfile" File="./rsyslog.input.alt/alt*file"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="'"'"', ")
  property(name="$!metadata!filename")
  constant(value="\n")
}

if $msg contains "msgnum:" then
	action( type="omfile" file="rsyslog.out.log" template="outfmt")

#*.* action(type="omfile" file="rsyslog.debug")
'

# create first directory and file before startup, so ensure we will NOT
# get an initial inotify notify for it!
mkdir rsyslog.input.alt
#./inputfilegen -m 1 > rsyslog.input.alt/altfile.logfile
mkdir rsyslog.input.dir1
# the following is INVALID, as this is a file, but must be a directory!
./inputfilegen -m 1 > rsyslog.input.dir0

. $srcdir/diag.sh startup 

for j in `seq 1 $IMFILEINPUTFILESSTEPS`;
do
	echo "Loop Num $j"

	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		mkdir rsyslog.input.dir$i
		mkdir rsyslog.input.dir$i/dir$i
		./inputfilegen -m 1 > rsyslog.input.dir$i/dir$i/file.logfile
	done

	ls -d rsyslog.input.*
	
	# Check correct amount of input files each time
	let IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $j))
	. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILESALL $IMFILECHECKTIMEOUT

	# Delete all but first!
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		# slow systems (NFS) do not reliably do rm -r (unfortunately...)
		rm -rf rsyslog.input.dir$i/dir$i/file.logfile
		./msleep 100
		rm -rf rsyslog.input.dir$i/dir$i
		./msleep 100
		rm -vrf rsyslog.input.dir$i
	done

done

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
#echo rsyslog.debug:
#cat rsyslog.debug
. $srcdir/diag.sh exit
