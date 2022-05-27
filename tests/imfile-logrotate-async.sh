#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh check-inotify-only
. ${srcdir:=.}/diag.sh init
check_command_available logrotate
export NUMMESSAGES=10000
export RETRIES=50

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

global( debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"]
	)

module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="2")

input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input*.log" Tag="file:"
	Severity="error" Facility="local7" addMetadata="on" reopenOnTruncate="on")

$template outfmt,"%msg:F,58:2%\n"
if $msg contains "msgnum:" then
   action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

./inputfilegen -m $NUMMESSAGES -S 5 -B 500 -f $RSYSLOG_DYNNAME.input.log &
INPUTFILEGEN_PID=$!
echo "$INPUTFILEGEN_PID" > $RSYSLOG_DYNNAME.inputfilegen_pid



./msleep 1
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
./msleep 20
echo INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
./msleep 20
echo INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
logrotate --state $RSYSLOG_DYNNAME.logrotate.state -f $RSYSLOG_DYNNAME.logrotate
echo INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*
echo ls ${RSYSLOG_DYNNAME}.spool:
ls -li ${RSYSLOG_DYNNAME}.spool
echo INPUT FILES:
ls -li $RSYSLOG_DYNNAME.input*

# generate more input after logrotate into new logfile
#./inputfilegen -m $TESTMESSAGES -i $TESTMESSAGES >> $RSYSLOG_DYNNAME.input.1.log
#ls -l $RSYSLOG_DYNNAME.input*

#msgcount=$((2* TESTMESSAGES))
#wait_file_lines $RSYSLOG_OUT_LOG $msgcount $RETRIES
wait_file_lines

shutdown_when_empty
wait_shutdown
seq_check 
#seq_check 0 $TESTMESSAGESFULL
exit_test
