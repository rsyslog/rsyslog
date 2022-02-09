#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
export TESTMESSAGES=10000
export RETRIES=50
export TESTMESSAGESFULL=19999

generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool

/* Filter out busy debug output */
global(
	debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
	)

module(	load="../plugins/imfile/.libs/imfile" 
	mode="inotify" 
	PollingInterval="1")

input(type="imfile"
	File="./'$RSYSLOG_DYNNAME'.input.*.log"
	Tag="file:"
	Severity="error"
	Facility="local7"
	addMetadata="on"
)
input(type="imfile"
	File="/does/not/exist/*.log"
	Tag="file:"
	Severity="error"
	Facility="local7"
	addMetadata="on"
)

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
if $msg contains "imfile:" then
 action(
   type="omfile"
   file="'$RSYSLOG_DYNNAME.errmsgs'"
 )
'

# generate input file first. 
./inputfilegen -m $TESTMESSAGES > $RSYSLOG_DYNNAME.input.1.log
ls -l $RSYSLOG_DYNNAME.input*

startup

# sleep a little to give rsyslog a chance to begin processing

wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES $RETRIES

# Move to another filename
mv $RSYSLOG_DYNNAME.input.1.log rsyslog.input.2.log

./msleep 500

# Write into the renamed file
echo 'testmessage1
testmessage2' >> rsyslog.input.2.log

./msleep 500

if grep "imfile: internal error? inotify provided watch descriptor" < "$RSYSLOG_DYNNAME.errmsgs" ; then
        echo "Error: inotify event from renamed file"
        exit 1
fi

# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.2.log
ls -l $RSYSLOG_DYNNAME.input*
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -l ${RSYSLOG_DYNNAME}.spool
./msleep 500

let msgcount="2* $TESTMESSAGES"
wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 $TESTMESSAGESFULL
exit_test
