#!/bin/bash
# added 2019-02-28 by rgerhards
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
# generate a very small file so that imfile cannot generate file_id
./inputfilegen -m1 > $RSYSLOG_DYNNAME.input
startup
shutdown_when_empty
wait_shutdown
seq_check 0 0 # check we got the (one) message correctly
# and also check state file name is correct:
inode=$(get_inode "$RSYSLOG_DYNNAME.input")
if [ ! -f "$RSYSLOG_DYNNAME.spool/imfile-state:$inode" ]; then
	printf 'FAIL: state file name incorrect,\nexpected \"%s\"\nspool dir is:\n' \
		$RSYSLOG_DYNNAME.spool/imfile-state:$inode
	ls -l $RSYSLOG_DYNNAME.spool
	error_exit 1
fi

printf '\nSTAGE 1 OK - inode-only state file properly generated\n\n'

# now add data to the input file, up to a point where we have file_id.
# rsyslog must update state file name AND remove inode-only one.
./inputfilegen -m999 -i1 >> $RSYSLOG_DYNNAME.input
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 1000
shutdown_when_empty
wait_shutdown

seq_check 0 999 # check we got the messages correctly
# and verify the state files are correct
if [ -f "$RSYSLOG_DYNNAME.spool/imfile-state:$inode" ]; then
	printf 'FAIL: inode-only state file still exists, but must not.\nincorrect name: \"%s\"\nspool dir is:\n' \
		$RSYSLOG_DYNNAME.spool/imfile-state:$inode
	ls -l $RSYSLOG_DYNNAME.spool
	error_exit 1
fi
	ls -l $RSYSLOG_DYNNAME.spool
exit_test
