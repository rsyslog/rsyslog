#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
export TESTMESSAGES=10000
export RETRIES=10
export TESTMESSAGESFULL=19999
echo [imfile-rename.sh]
. $srcdir/diag.sh check-inotify-only
. $srcdir/diag.sh init
generate_conf
add_conf '
$WorkDirectory test-spool

/* Filter out busy debug output */
global(
	debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
	)

module(	load="../plugins/imfile/.libs/imfile" 
	mode="inotify" 
	PollingInterval="1")

input(type="imfile"
	File="./rsyslog.input.*.log"
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
'

# generate input file first. 
./inputfilegen -m $TESTMESSAGES > rsyslog.input.1.log
ls -li rsyslog.input*

startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

# Move to another filename
mv rsyslog.input.1.log rsyslog.input.2.log

# generate some more input into moved file 
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> rsyslog.input.2.log
ls -li rsyslog.input*
echo ls test-spool:
ls -l test-spool

startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

seq_check 0 $TESTMESSAGESFULL
wc rsyslog.out.log
exit_test
