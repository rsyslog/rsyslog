#!/bin/bash
# This test creates multiple symlinks (all watched by rsyslog via wildcard)
# chained to target files via additional symlinks and checks that all files
# are recorded with correct corresponding metadata (name of symlink 
# matching configuration).
# This is part of the rsyslog testbench, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
export IMFILEINPUTFILES="10"
export IMFILELASTINPUTLINES="3"
export IMFILECHECKTIMEOUT="60"

# generate input files first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
mkdir $RSYSLOG_DYNNAME.statefiles
generate_conf
add_conf '
# comment out if you need more debug info:
	global( debug.whitelist="on"
		debug.files=["imfile.c"])
module(load="../plugins/imfile/.libs/imfile"
      statefile.directory="'${RSYSLOG_DYNNAME}'.statefiles"
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

./inputfilegen -m 1 > $RSYSLOG_DYNNAME.input-symlink.log
mkdir $RSYSLOG_DYNNAME.targets

# Start rsyslog now before adding more files
startup

for i in $(seq 2 $IMFILEINPUTFILES);
do
	printf '\ncreating %s\n' $RSYSLOG_DYNNAME.targets/$i.log
	./inputfilegen -m 1 -i $((i-1)) > $RSYSLOG_DYNNAME.targets/$i.log
	ls -l $RSYSLOG_DYNNAME.targets/$i.log
	ln -sv $RSYSLOG_DYNNAME.targets/$i.log $RSYSLOG_DYNNAME.link.$i.log
	ln -sv $RSYSLOG_DYNNAME.link.$i.log $RSYSLOG_DYNNAME.input.$i.log
	printf '%s generated file %s\n' "$(tb_timestamp)" "$i"
	ls -l $RSYSLOG_DYNNAME.link.$i.log
	# wait until this file has been processed
	content_check_with_count "HEADER msgnum:000000" $i $IMFILECHECKTIMEOUT
done

./inputfilegen -m $IMFILELASTINPUTLINES > $RSYSLOG_DYNNAME.input.$((IMFILEINPUTFILES + 1)).log
ls -l $RSYSLOG_DYNNAME.input.* $RSYSLOG_DYNNAME.link.* $RSYSLOG_DYNNAME.targets

# Content check with timeout
content_check_with_count "input.11.log" $IMFILELASTINPUTLINES $IMFILECHECKTIMEOUT

shutdown_when_empty
wait_shutdown
exit_test
