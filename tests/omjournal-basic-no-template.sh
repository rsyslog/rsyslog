#!/bin/bash
# a basic test for omjournal.
# addd 2016-03-18 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh require-journalctl
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omjournal/.libs/omjournal")
input(type="imtcp" port="13514")

action(type="omjournal")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<133>2011-03-01T11:22:12Z host tag msgh RsysLoG-TESTBENCH $COOKIE\""
./msleep 500
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
# if we reach this, we have at least not aborted
journalctl -r -t rsyslogd:  |grep "RsysLoG-TESTBENCH $COOKIE"
if [ $? -ne 1 ]; then
	echo "error: cookie $COOKIE not found. Head of journal:"
	journalctrl -r -t rsyslogd: | head
	exit 1
fi
. $srcdir/diag.sh exit
