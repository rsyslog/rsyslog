#!/bin/bash
# This is part of the rsyslog testbench, licensed under GPLv3
. $srcdir/diag.sh init
export TESTMESSAGES=10000
export RETRIES=10
export TESTMESSAGESFULL=19999
echo [imfile-rename.sh]
. $srcdir/diag.sh check-inotify-only
generate_conf
add_conf '
global(workDirectory="test-spool")
module(load="../plugins/imfile/.libs/imfile" mode="inotify" PollingInterval="1")

/* Filter out busy debug output */
global( debug.whitelist="off"
	debug.files=["rainerscript.c", "ratelimit.c", "ruleset.c", "main Q", "msg.c", "../action.c"])

input(type="imfile" File="./rsyslog.input"
	Tag="file:" Severity="error" Facility="local7" addMetadata="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

# generate small input file - state file must be inode only
./inputfilegen -m 1 > rsyslog.input
ls -li rsyslog.input

echo "STEP 1 - small input"
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

echo "STEP 2 - still small input"
# add a bit to input file, but state file must still be inode only
./inputfilegen -m 1 -i1 >> rsyslog.input
ls -li rsyslog.input*
if [ $(ls test-spool/* | wc -l) -ne 1 ]; then
	echo FAIL: more than one state file in work directory:
	ls -l test-spool
	error_exit 1
fi

startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

echo "STEP 3 - larger input, hash shall be used"
./inputfilegen -m 998 TESTMESSAGES -i 2 >> rsyslog.input
ls -li rsyslog.input*
echo ls test-spool:
ls -l test-spool

startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

if [ $(ls test-spool/* | wc -l) -ne 1 ]; then
	echo FAIL: more than one state file in work directory:
	ls -l test-spool
	error_exit 1
fi


echo "STEP 4 - append to larger input, hash state file must now be found"
./inputfilegen -m 1000 TESTMESSAGES -i 1000 >> rsyslog.input
ls -li rsyslog.input*
echo ls test-spool:
ls -l test-spool

startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!

if [ $(ls test-spool/* | wc -l) -ne 1 ]; then
	echo FAIL: more than one state file in work directory:
	ls -l test-spool
	error_exit 1
fi

seq_check 0 1999
exit_test
