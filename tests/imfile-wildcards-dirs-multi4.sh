#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="1"
export IMFILEINPUTFILESSTEPS="5"
#export IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $IMFILEINPUTFILESSTEPS))
export IMFILECHECKTIMEOUT="20"
. $srcdir/diag.sh init
generate_conf
add_conf '
$WorkDirectory test-spool

/* Filter out busy debug output, comment out if needed */
global(
	debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
)

module(	load="../plugins/imfile/.libs/imfile" 
	mode="inotify" 
	PollingInterval="1")

input(type="imfile"
	File="./rsyslog.input.dir1/*/*/*/*/*/file.logfile"
	Tag="file:"
	Severity="error"
	Facility="local7"
	addMetadata="on"
)

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="'
add_conf "'"
add_conf ', ")
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
. $srcdir/diag.sh check-inotify
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
startup

for i in `seq 1 $IMFILEINPUTFILES`;
do
	echo "Make rsyslog.input.dir$i"
	mkdir rsyslog.input.dir$i
done

for j in `seq 1 $IMFILEINPUTFILESSTEPS`;
do
	echo "Loop Num $j"
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		echo "Make rsyslog.input.dir$i/dir$j/testdir"
		mkdir rsyslog.input.dir$i/dir$j
		mkdir rsyslog.input.dir$i/dir$j/testdir
		mkdir rsyslog.input.dir$i/dir$j/testdir/su$j
		mkdir rsyslog.input.dir$i/dir$j/testdir/su$j/bd$j
		mkdir rsyslog.input.dir$i/dir$j/testdir/su$j/bd$j/ir$j
		./inputfilegen -m 1 > rsyslog.input.dir$i/dir$j/testdir/su$j/bd$j/ir$j/file.logfile
	done
	ls -d rsyslog.input.*

	# Check correct amount of input files each time
	let IMFILEINPUTFILESALL=$(($IMFILEINPUTFILES * $j))
	. $srcdir/diag.sh content-check-with-count "HEADER msgnum:00000000:" $IMFILEINPUTFILESALL $IMFILECHECKTIMEOUT

	# Delete all but first!
	for i in `seq 1 $IMFILEINPUTFILES`;
	do
		rm -rf rsyslog.input.dir$i/dir$j/testdir/su$j/bd$j/ir$j/file.logfile
		rm -rf rsyslog.input.dir$i/dir$j
	done
done

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
exit_test
