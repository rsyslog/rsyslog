#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate
export NUMMESSAGES=10000
export RETRIES=50

# Uncomment fdor debuglogs
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

# Write logrotate config file
echo '"./'$RSYSLOG_DYNNAME'.input*.log"
{
    #daily
    rotate 60
    missingok
    notifempty
    sharedscripts
    postrotate
    kill -HUP $(cat '$RSYSLOG_DYNNAME'.inputfilegen_pid)
    endscript
    #olddir /logs/old

}' > $RSYSLOG_DYNNAME.logrotate


generate_conf
add_conf '
$WorkDirectory '$RSYSLOG_DYNNAME'.spool

global( debug.whitelist="on"
	debug.files=["imfile.c", "stream.c"]
	)

module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="2")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on" reopenOnTruncate="on")

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
   action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

./inputfilegen -m $NUMMESSAGES -S 5 -B 100 -I 1000 -f $RSYSLOG_DYNNAME.input.log &
INPUTFILEGEN_PID=$!
echo "$INPUTFILEGEN_PID" > $RSYSLOG_DYNNAME.inputfilegen_pid

./msleep 1
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
./msleep 20
echo ======================:
echo ROTATE 1	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
./msleep 20
echo ======================:
echo ROTATE 2	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
echo ======================:
echo ROTATE 3	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
echo ======================:
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -li ${RSYSLOG_DYNNAME}.spool
echo ======================:
echo FINAL	INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*

# generate more input after logrotate into new logfile
#./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.1.log
#ls -l $RSYSLOG_DYNNAME.input*

#msgcount=$((2* TESTMESSAGES))
#wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES
# Output extra information
./msleep 1000
echo ======================:
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log)
echo TAIL	$RSYSLOG_DYNNAME.input.log:
tail $RSYSLOG_DYNNAME.input.log
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log.1)
echo TAIL	$RSYSLOG_DYNNAME.input.log.1:
tail $RSYSLOG_DYNNAME.input.log.1
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.input.log.2)
echo TAIL	$RSYSLOG_DYNNAME.input.log.2:
tail $RSYSLOG_DYNNAME.input.log.2
echo ""
echo LINES:	$(wc -l $RSYSLOG_DYNNAME.inpt.log.3)
echo TAIL	$RSYSLOG_DYNNAME.input.log.3:
tail $RSYSLOG_DYNNAME.input.log.3
echo ""
wait_file_lines

touch $RSYSLOG_DYNNAME.input.log
./msleep 1000

shutdown_when_empty
wait_shutdown
seq_check 
#seq_check 0 $TESTMESSAGESFULL
exit_test
