#!/bin/bash
# added 2016-11-02 by rgerhards
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo [imfile-persist-state-1.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
generate_conf
add_conf '
global(workDirectory="test-spool")

module(load="../plugins/imfile/.libs/imfile")

input(	type="imfile"
	file="./rsyslog.input"
	tag="file:"
	startmsg.regex="^msgnum"
	PersistStateInterval="1"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen 5 4000 > rsyslog.input
startup
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown	# we need to wait until rsyslogd is finished!
seq_check 0 3
exit_test
