#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate

export IMFILEROTATES="10"
export TESTMESSAGES=10000
export TESTMESSAGESFULL=$((IMFILEROTATES * TESTMESSAGES-1))

generate_conf
add_conf '
global(
	workDirectory="'$RSYSLOG_DYNNAME'.spool"
	/* Filter out busy debug output */
	debug.whitelist="off"
	debug.files=["omfile.c", "queue.c", "rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
	)

module(load="../plugins/imfile/.libs/imfile" mode="inotify")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'

# Write logrotate config file
echo '"./'$RSYSLOG_DYNNAME'.input"
{
	create
	daily
	missingok
	rotate 14
	notifempty
	compress
	delaycompress
}' > $RSYSLOG_DYNNAME.logrotate


display_file() {
	printf '\nFILE %s content:\n' $1
	cat -n $1
}

startup

TESTMESSAGESSTART=0
for i in $(seq 1 $IMFILEROTATES);
do
	#printf 'PRESS ENTER TO CONTINUE\n'
	#read
	printf '\n\nNEW RUN:\n'

	./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGESSTART > $RSYSLOG_DYNNAME.input

	ls -li $RSYSLOG_DYNNAME.input*
	echo ls ${RSYSLOG_DYNNAME}.spool:
	ls -li ${RSYSLOG_DYNNAME}.spool
	echo STATE FILE CONTENT:
	shopt -s extglob
	for filename in "$RSYSLOG_DYNNAME.spool"/imfile-state:*; do display_file $filename; done

	# Wait until testmessages are processed by imfile!
	msgcount=$((i * TESTMESSAGES-1))
	# echo "TESTMESSAGESSTART: $TESTMESSAGESSTART - TotalMsgCount: $msgcount"
	wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES

	# Logrotate on logfile
	logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate

	TESTMESSAGESSTART=$((TESTMESSAGESSTART+TESTMESSAGES))
done


shutdown_when_empty
wait_shutdown
seq_check 0 $TESTMESSAGESFULL
exit_test
