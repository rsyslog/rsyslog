#!/bin/bash
# This test points multiple symlinks (all watched by rsyslog via wildcard)
# to single file and checks that message is reported once for each symlink
# with correct corresponding metadata.
# This is part of the rsyslog testbench, released under ASL 2.0
export IMFILEINPUTFILES="10"
echo [imfile-symlink.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

imfilebefore="rsyslog.input-symlink.log"
./inputfilegen -m 1 > $imfilebefore
mkdir targets

generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])
module(load="../plugins/imfile/.libs/imfile"
       mode="inotify" normalizePath="off")
input(type="imfile" File="./rsyslog.input-symlink.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on")
input(type="imfile" File="./rsyslog.input.*.log" Tag="file:"
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
# Start rsyslog now before adding more files
startup

cp $imfilebefore targets/target.log
for i in `seq 2 $IMFILEINPUTFILES`;
do
	ln -s targets/target.log rsyslog.input.$i.log
	# Wait little for correct timing
	./msleep 50
done

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown        # we need to wait until rsyslogd is finished!

sort ${RSYSLOG_OUT_LOG} > ${RSYSLOG_OUT_LOG}.sorted

{
	echo HEADER msgnum:00000000:, filename: ./rsyslog.input-symlink.log, fileoffset: 0
	for i in `seq 2 $IMFILEINPUTFILES` ; do
		echo HEADER msgnum:00000000:, filename: ./rsyslog.input.${i}.log, fileoffset: 0
	done
} | sort | cmp - ${RSYSLOG_OUT_LOG}.sorted
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, ${RSYSLOG_OUT_LOG} is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
