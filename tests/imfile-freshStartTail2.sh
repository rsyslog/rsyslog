#!/bin/bash
# Verifies that freshStartTail still reads a wildcard-matched file created
# after startup and continues to follow later appends to that same file.
# Added 2018-05-17 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
export IMFILECHECKTIMEOUT="60"

mkdir $RSYSLOG_DYNNAME.spool
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" freshStartTail="on" Tag="pro"
	File="'$RSYSLOG_DYNNAME'.input.*")

template(name="outfmt" type="string" string="%msg%\n")

:syslogtag, contains, "pro" action(type="omfile" File=`echo $RSYSLOG_OUT_LOG`
	template="outfmt")
'
startup
# Use a canary post-startup wildcard file to prove imfile has armed the
# directory watch before creating the file used for the append-following check.
echo '{ "id": "canary"}' > $RSYSLOG_DYNNAME.input.canary
content_check_with_count '{ "id": "canary"}' 1 $IMFILECHECKTIMEOUT

echo '{ "id": "jinqiao1"}' > $RSYSLOG_DYNNAME.input.a
content_check_with_count '{ "id": "jinqiao1"}' 1 $IMFILECHECKTIMEOUT
wait_queueempty
# Let imfile settle after the first read before checking that later appends to
# the same post-startup wildcard file are followed.
./msleep 1000

echo '{ "id": "jinqiao2"}' >> $RSYSLOG_DYNNAME.input.a
content_check_with_count '{ "id": "jinqiao2"}' 1 $IMFILECHECKTIMEOUT

shutdown_when_empty
wait_shutdown
exit_test
