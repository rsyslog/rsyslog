#!/bin/bash
# Verifies imfile reopenOnTruncate with logrotate copytruncate. The oracle is
# the exact sequence 0..19999: rsyslog must read the pre-rotate file, finish
# that batch before the destructive copytruncate, notice the truncation, resume
# with the post-truncate sentinel, and then read all remaining post-rotate data.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate

export TESTMESSAGES=10000
export RETRIES=50
export TESTMESSAGESFULL=19999
export POST_ROTATE_SENTINEL=1

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
	reopenOnTruncate="on"
)

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'

# Write logrotate config file
echo '"./'$RSYSLOG_DYNNAME'.input.*.log"
{
	su '"$(id -un)"' '"$(id -gn)"'
	rotate 7
	create
	daily
	missingok
	notifempty
	compress
	copytruncate
}' > $RSYSLOG_DYNNAME.logrotate

# generate input file first.
./inputfilegen -m $TESTMESSAGES > $RSYSLOG_DYNNAME.input.1.log
ls -l $RSYSLOG_DYNNAME.input*

startup

# Wait until the initial file content is visible, then drain the main queue
# before copytruncate so the test rotates only after the pre-rotate batch is
# fully processed.
wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES $RETRIES
wait_queueempty

# Logrotate on logfile
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate

# First write a small sentinel after copytruncate and wait for it. This keeps
# the test focused on the invariant that imfile notices the truncation and
# resumes reading before the large follow-up batch is appended.
./inputfilegen -m $POST_ROTATE_SENTINEL -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.1.log
wait_file_lines $RSYSLOG_OUT_LOG $((TESTMESSAGES + POST_ROTATE_SENTINEL)) $RETRIES

# generate more input after logrotate into new logfile
./inputfilegen -m $((TESTMESSAGES - POST_ROTATE_SENTINEL)) -i $((TESTMESSAGES + POST_ROTATE_SENTINEL)) >> $RSYSLOG_DYNNAME.input.1.log
ls -l $RSYSLOG_DYNNAME.input*
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -l ${RSYSLOG_DYNNAME}.spool

msgcount=$((2* TESTMESSAGES))
wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 $TESTMESSAGESFULL
exit_test
