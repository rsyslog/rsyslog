#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
export IMFILEINPUTFILES="10"
export IMFILELASTINPUTLINES="3"
export IMFILECHECKTIMEOUT="60"

mkdir "$RSYSLOG_DYNNAME.work"
generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])

global(workDirectory="./'"$RSYSLOG_DYNNAME"'.work")

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

imfilebefore=$RSYSLOG_DYNNAME.input.01.log
./inputfilegen -m 1 > $imfilebefore

# Start rsyslog now before adding more files
startup

for i in $(seq 2 $IMFILEINPUTFILES);
do
	filnbr=$(printf "%2.2d" $i)
	cp $imfilebefore $RSYSLOG_DYNNAME.input.$filnbr.log
	imfilebefore=$RSYSLOG_DYNNAME.input.$filnbr.log
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

# Sort Output file now, and compare full file content
presort

printf 'HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.01.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.02.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.03.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.04.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.05.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.06.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.07.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.08.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.09.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.10.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./'$RSYSLOG_DYNNAME'.input.11.log, fileoffset: 0
HEADER msgnum:00000001:, filename: ./'$RSYSLOG_DYNNAME'.input.11.log, fileoffset: 17
HEADER msgnum:00000002:, filename: ./'$RSYSLOG_DYNNAME'.input.11.log, fileoffset: 34\n' | cmp - $RSYSLOG_DYNNAME.presort
if [ ! $? -eq 0 ]; then
	echo "FAIL: invalid output generated, $RSYSLOG_DYNNAME.presort is:"
	echo "File contents:"
	cat -n $RSYSLOG_DYNNAME.presort
	error_exit 1
fi;

exit_test
