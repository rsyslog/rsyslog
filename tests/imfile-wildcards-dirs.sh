#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. $srcdir/diag.sh check-inotify
. ${srcdir:=.}/diag.sh init
export IMFILEINPUTFILES="10"
export IMFILECHECKTIMEOUT="60"

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
/* Filter out busy debug output, comment out if needed */
global(
	debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
)

global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")

module(	load="../plugins/imfile/.libs/imfile" 
	mode="inotify" 
	PollingInterval="1")

input(type="imfile"
	File="./'$RSYSLOG_DYNNAME'.input.*/*.logfile"
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

for i in $(seq 1 $IMFILEINPUTFILES);
do
	mkdir $RSYSLOG_DYNNAME.input.dir$i
	touch $RSYSLOG_DYNNAME.input.dir$i/file.logfile
	./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.dir$i/file.logfile
done
ls -d $RSYSLOG_DYNNAME.input.*

# Content check with timeout
content_check_with_count "HEADER msgnum:00000000:" $IMFILEINPUTFILES $IMFILECHECKTIMEOUT

for i in $(seq 1 $IMFILEINPUTFILES);
do
	rm -rf $RSYSLOG_DYNNAME.input.dir$i/
done

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
exit_test
