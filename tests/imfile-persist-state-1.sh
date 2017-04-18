#!/bin/bash
# added 2016-11-02 by rgerhards
# This is part of the rsyslog testbench, licensed under ASL 2.0

uname
if [ `uname` = "SunOS" ] ; then
   echo "Solaris does not support inotify."
   exit 77
fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
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
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen 5 4000 > rsyslog.input
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
. $srcdir/diag.sh seq-check 0 3
. $srcdir/diag.sh exit
