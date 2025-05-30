#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify-only
export IMFILEINPUTFILES="10"
export IMFILEINPUTFILESSTEPS="5"
#export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))
export IMFILECHECKTIMEOUT="60"

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '

/* Filter out busy debug output, comment out if needed */
/*
global(
	debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
)
*/

global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")

module(	load="../plugins/imfile/.libs/imfile" 
	mode="inotify" 
	PollingInterval="1")

input(type="imfile"
	File="./'$RSYSLOG_DYNNAME'.input.*/*/*.logfile"
	Tag="file:"
	Severity="error"
	Facility="local7"
	addMetadata="on"
)

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value=", ")
  property(name="$!metadata!filename")
  constant(value="\n")
}

if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
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
		rm -rf $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
		rm -rf $RSYSLOG_DYNNAME.input.dir$i/dir$i/
		rm -rf $RSYSLOG_DYNNAME.input.dir$i
	done

	# Helps in testbench parallel mode. 
	#	Otherwise sometimes directories are not marked deleted in imfile before they get created again.
	#	This is properly a real issue in imfile when FILE IO is high. 
	./msleep 1000
done

shutdown_when_empty
wait_shutdown
exit_test
