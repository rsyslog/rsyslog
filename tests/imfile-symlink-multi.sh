#!/bin/bash
# Verifies imfile wildcard handling for multiple symlinks to one target file.
# The oracle is exact output metadata: rsyslog must report one record for the
# original watched file and one record for each wildcard symlink, each with the
# corresponding symlink filename and file offset.
# This is part of the rsyslog testbench, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
export IMFILEINPUTFILES="10"
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
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input-symlink.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input.*.log" Tag="file:"
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
	action( type="omfile" file="'${RSYSLOG_OUT_LOG}'" template="outfmt")
'
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

imfilebefore="$RSYSLOG_DYNNAME.input-symlink.log"
./inputfilegen -m 1 > $imfilebefore
mkdir $RSYSLOG_DYNNAME.targets
# Start rsyslog now before adding more files
startup

cp $imfilebefore $RSYSLOG_DYNNAME.targets/target.log
for i in $(seq 2 $IMFILEINPUTFILES);
do
	ln -s $RSYSLOG_DYNNAME.targets/target.log $RSYSLOG_DYNNAME.input.$i.log
	# Let imfile observe each newly-created symlink before adding the next.
	./msleep 50
done

# Symlink creation is asynchronous from imfile's point of view. Wait until all
# expected records are visible before shutdown, then keep the exact comparison.
content_check_with_count "HEADER msgnum:000000" $IMFILEINPUTFILES $IMFILECHECKTIMEOUT
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown        # we need to wait until rsyslogd is finished!

sort ${RSYSLOG_OUT_LOG} > ${RSYSLOG_OUT_LOG}.sorted

echo HEADER msgnum:00000000:, filename: ./$RSYSLOG_DYNNAME.input-symlink.log, fileoffset: 0 > $RSYSLOG_DYNNAME.expected
for i in $(seq 2 $IMFILEINPUTFILES) ; do
	echo HEADER msgnum:00000000:, filename: ./$RSYSLOG_DYNNAME.input.${i}.log, fileoffset: 0 >> $RSYSLOG_DYNNAME.expected
done
sort < $RSYSLOG_DYNNAME.expected | cmp - ${RSYSLOG_OUT_LOG}.sorted
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, ${RSYSLOG_OUT_LOG} is:"
  cat -n $RSYSLOG_OUT_LOG
  echo EXPECTED:
  cat -n $RSYSLOG_DYNNAME.expected
  error_exit 1
fi;

exit_test
