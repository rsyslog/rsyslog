#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export IMFILEINPUTFILES="8"
export IMFILEINPUTFILESSTEPS="5"
export IMFILECHECKTIMEOUT="15"
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

# Start rsyslog now before adding more files
generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool

global( debug.whitelist="on"
	debug.files=["imfile.c", "stream.c"])
#	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q",
#	"msg.c", "../action.c", "imdiag.c"])

module(load="../plugins/imfile/.libs/imfile" mode="polling" pollingInterval="1")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.dir?/*/*.logfile"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.alt/alt*file"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value=", ")
  property(name="$!metadata!filename")
  constant(value="\n")
}

if $msg contains "msgnum:" then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

# create first directory and file before startup, so ensure we will NOT
# get an initial inotify notify for it!
#mkdir $RSYSLOG_DYNNAME.input.alt
#./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.alt/altfile.logfile
#mkdir $RSYSLOG_DYNNAME.input.dir1
# the following is INVALID, as this is a file, but must be a directory!
#./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input.dir0

startup

for j in $(seq 1 $IMFILEINPUTFILESSTEPS); do
	cp log log.$j
	echo "Loop Num $j"

	for i in $(seq 1 $IMFILEINPUTFILES);
	do
		mkdir $RSYSLOG_DYNNAME.input.dir$i
		mkdir $RSYSLOG_DYNNAME.input.dir$i/dir$i
		# proposed by AL - why? touch $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
		./inputfilegen -i $j -m 1 > $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
	done

	ls -d $RSYSLOG_DYNNAME.input.*
	
	# Check correct amount of input files each time
	IMFILEINPUTFILESALL=$((IMFILEINPUTFILES * j))
	content_check_with_count "HEADER msgnum:000000" $IMFILEINPUTFILESALL $IMFILECHECKTIMEOUT

	# Check correct amount of state files each time: 0 because input file size < 512
	check_spool_count 0 "$(stat -c "%n: %i" $RSYSLOG_DYNNAME.input.dir*/dir*/file.logfile)"

	# Delete all but first!
	for i in $(seq 1 $IMFILEINPUTFILES);
	do
		rm -rf $RSYSLOG_DYNNAME.input.dir$i/dir$i/file.logfile
		rm -rf $RSYSLOG_DYNNAME.input.dir$i
	done
done

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
exit_test
