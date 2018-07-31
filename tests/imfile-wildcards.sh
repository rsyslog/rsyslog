#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export IMFILEINPUTFILES="10"
echo [imfile-wildcards.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).

imfilebefore="rsyslog.input.1.log"
./inputfilegen -m 1 > $imfilebefore

generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])

module(load="../plugins/imfile/.libs/imfile"
       mode="inotify" normalizePath="off")

input(type="imfile" File="./rsyslog.input.*.log" Tag="file:"
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
# Start rsyslog now before adding more files
startup

for i in `seq 2 $IMFILEINPUTFILES`;
do
	cp $imfilebefore rsyslog.input.$i.log
	imfilebefore="rsyslog.input.$i.log"
	# Wait little for correct timing
	./msleep 50
done
./inputfilegen -m 3 > rsyslog.input.$((IMFILEINPUTFILES + 1)).log
ls -l rsyslog.input.*

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

printf 'HEADER msgnum:00000000:, filename: ./rsyslog.input.1.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.2.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.3.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.4.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.5.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.6.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.7.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.8.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.9.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.10.log, fileoffset: 0
HEADER msgnum:00000000:, filename: ./rsyslog.input.11.log, fileoffset: 0
HEADER msgnum:00000001:, filename: ./rsyslog.input.11.log, fileoffset: 17
HEADER msgnum:00000002:, filename: ./rsyslog.input.11.log, fileoffset: 34\n' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  exit 1
fi;

exit_test
