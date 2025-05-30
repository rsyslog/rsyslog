#!/bin/bash
# added 2019-04-25 by rgerhards
# check that the "statfile.directory" module parameter is accepted
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
mkdir $RSYSLOG_DYNNAME.statefiles
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile"
      statefile.directory="'${RSYSLOG_DYNNAME}'.statefiles")
input(type="imfile" tag="file:" file="./'$RSYSLOG_DYNNAME'.input")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
# generate a very small file so that imfile cannot generate file_id (simple case)
./inputfilegen -m1 > $RSYSLOG_DYNNAME.input
startup
shutdown_when_empty
wait_shutdown
seq_check 0 0 # check we got the messages correctly
# and also check state file name is correct:
# shellcheck disable=SC2012  - we do not display (or even use) the file name
inode=$(ls -i "$RSYSLOG_DYNNAME.input"|awk '{print $1}')
if [ ! -f "$RSYSLOG_DYNNAME.statefiles/imfile-state:$inode" ]; then
	printf 'FAIL: state file name incorrect,\nexpected \"%s\"\nstatefiles dir is:\n' \
		$RSYSLOG_DYNNAME.statefiles/imfile-state:$inode
	ls -l $RSYSLOG_DYNNAME.statefiles
	error_exit 1
fi
exit_test
