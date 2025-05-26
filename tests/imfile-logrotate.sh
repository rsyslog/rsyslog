#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate

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
	rotate 7
	create
	daily
	missingok
	notifempty
	compress
}' > $RSYSLOG_DYNNAME.logrotate

# generate input file first.
./inputfilegen -m $TESTMESSAGES > $RSYSLOG_DYNNAME.input.1.log
ls -l $RSYSLOG_DYNNAME.input*

startup

# Wait until testmessages are processed by imfile!
wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES $RETRIES

# Logrotate on logfile
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate

# generate more input after logrotate into new logfile
./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.1.log
ls -l $RSYSLOG_DYNNAME.input*
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -l ${RSYSLOG_DYNNAME}.spool

msgcount=$((2* TESTMESSAGES))
wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 $TESTMESSAGESFULL
exit_test
