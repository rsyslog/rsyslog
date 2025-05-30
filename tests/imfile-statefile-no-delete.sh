#!/bin/bash
# Added 2019-02-28
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
export TESTMESSAGES=1000
export TESTMESSAGESFULL=999
export RETRIES=50

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="1")
input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input" deleteStateOnFileDelete="off")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
./inputfilegen -m $TESTMESSAGES > $RSYSLOG_DYNNAME.input
inode=$(get_inode "$RSYSLOG_DYNNAME.input")
startup
wait_file_lines $RSYSLOG_OUT_LOG $TESTMESSAGES $RETRIES
rm $RSYSLOG_DYNNAME.input
# sleep a little to give rsyslog a chance to notice the deleted file
./msleep 2000

shutdown_when_empty
wait_shutdown
seq_check 0 $TESTMESSAGESFULL # check we got the message correctly
if ! ls $RSYSLOG_DYNNAME.spool/imfile-state:$inode:* 1> /dev/null 2>&1; then
	printf 'FAIL: state file was deleted when it should not have been\nspool dir is:\n'
	ls -l $RSYSLOG_DYNNAME.spool
	error_exit 1
fi

exit_test
