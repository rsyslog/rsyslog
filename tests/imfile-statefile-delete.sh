#!/bin/bash
# added 2019-02-28
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export TESTMESSAGES=1000
export TESTMESSAGESFULL=999 
export RETRIES=50

# Uncomment fdor debuglogs
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="1")
input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
./inputfilegen -m $TESTMESSAGES > $RSYSLOG_DYNNAME.input
inode=$(get_inode "$RSYSLOG_DYNNAME.input")
startup
wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES $RETRIES
rm $RSYSLOG_DYNNAME.input
sleep_time_ms=0
while ls $RSYSLOG_DYNNAME.spool/imfile-state:$inode:* 1> /dev/null 2>&1; do
	./msleep 100
	((sleep_time_ms+=100))
	if [ $sleep_time_ms -ge 6000 ]; then
		touch $RSYSLOG_DYNNAME:.tmp
	fi
	if [ $sleep_time_ms -ge 30000 ]; then
	        printf 'FAIL: state file still exists when it should have been deleted\nspool dir is:\n'
	        ls -l $RSYSLOG_DYNNAME.spool
	        error_exit 1
	fi
done
shutdown_when_empty
wait_shutdown
seq_check 0 $TESTMESSAGESFULL # check we got the  message correctly
exit_test
