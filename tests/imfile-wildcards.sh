#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="10"
export IMFILELASTINPUTLINES="3"
export IMFILECHECKTIMEOUT="20"

. $srcdir/diag.sh init
. $srcdir/diag.sh check-inotify
generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])

module(load="../plugins/imfile/.libs/imfile"
       mode="inotify" normalizePath="off")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on")

input(type="imfile" File="/does/not/exist/*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on")

template(name="outfmt" type="list") {
	constant(value="HEADER ")
	property(name="msg" format="json")
	constant(value=", filename: ")
	property(name="$!metadata!filename")
	constant(value=", fileoffset: ")
	property(name="$!metadata!fileoffset")
	constant(value="\n")
}

if $msg contains "msgnum:" then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

imfilebefore=$RSYSLOG_DYNNAME.input.1.log
./inputfilegen -m 1 > $imfilebefore

# Start rsyslog now before adding more files
startup

for i in `seq 2 $IMFILEINPUTFILES`;
do
	cp $imfilebefore $RSYSLOG_DYNNAME.input.$i.log
	imfilebefore=$RSYSLOG_DYNNAME.input.$i.log
done

# Content check with timeout
content_check_with_count "HEADER msgnum:00000000:" $IMFILEINPUTFILES $IMFILECHECKTIMEOUT

# Add some extra lines to the last log
./inputfilegen -m $IMFILELASTINPUTLINES > $RSYSLOG_DYNNAME.input.$((IMFILEINPUTFILES + 1)).log
ls -l $RSYSLOG_DYNNAME.input.*

# Content check with timeout
content_check_with_count "input.11.log" $IMFILELASTINPUTLINES $IMFILECHECKTIMEOUT

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
exit_test
