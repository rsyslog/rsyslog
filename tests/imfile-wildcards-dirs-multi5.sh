#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. ${srcdir:=.}/diag.sh init
export IMFILEINPUTFILES="8" #"8"
export IMFILEINPUTFILESSTEPS="5" #"5"
#export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))
export IMFILECHECKTIMEOUT="60"

# Start rsyslog now before adding more files
mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
global( workDirectory="./'"$RSYSLOG_DYNNAME"'.work"
        debug.whitelist="on"
	debug.files=["imfile.c"])

module(load="../plugins/imfile/.libs/imfile" mode="inotify" pollingInterval="1")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.dir?/*/*.logfile"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.alt/alt*file"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="'"'"', ")
  property(name="$!metadata!filename")
  constant(value="\n")
}

if $msg contains "msgnum:" then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")

#*.* action(type="omfile" file="rsyslog.debug")
'

# create first directory and file before startup, so ensure we will NOT
# get an initial inotify notify for it!
mkdir $RSYSLOG_DYNNAME.input.alt
#./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.alt/altfile.logfile
mkdir $RSYSLOG_DYNNAME.input.dir1
# the following is INVALID, as this is a file, but must be a directory!
./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.dir0

startup

for j in $(seq 1 $IMFILEINPUTFILESSTEPS);
do
	echo "Loop Num $j"

	for i in $(seq 1 $IMFILEINPUTFILES);
	do
		mkdir $RSYSLOG_DYNNAME.input.dir$i
		mkdir $RSYSLOG_DYNNAME.input.dir$i/dir$i
		touch $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
		./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
	done

	ls -d $RSYSLOG_DYNNAME.input.*
	
	# Check correct amount of input files each time
	IMFILEINPUTFILESALL=$((IMFILEINPUTFILES * j))
	content_check_with_count "HEADER msgnum:00000000:" $IMFILEINPUTFILESALL $IMFILECHECKTIMEOUT

	# Delete all but first!
	for i in $(seq 1 $IMFILEINPUTFILES);
	do
		# slow systems (NFS) do not reliably do rm -r (unfortunately...)
		rm -rf $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
		./msleep 100
		rm -rf $RSYSLOG_DYNNAME.input.dir$i/dir$i
		./msleep 100
		rm -vrf $RSYSLOG_DYNNAME.input.dir$i
	done

	# Helps in testbench parallel mode. 
	#	Otherwise sometimes directories are not marked deleted in imfile before they get created again.
	#	This is properly a real issue in imfile when FILE IO is high. 
	./msleep 1000
done

shutdown_when_empty
wait_shutdown
#echo rsyslog.debug:
#cat rsyslog.debug
exit_test
