#!/bin/bash
# Copyright 2014-11-20 by Rainer Gerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[empty-ruleset.sh\]: testing empty ruleset
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
$MainMsgQueueTimeoutShutdown 10000

input(type="imtcp" port="13514" ruleset="real")
input(type="imtcp" port="13515" ruleset="empty")

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!

ruleset(name="empty") {
}

ruleset(name="real") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
startup
. $srcdir/diag.sh tcpflood -p13515 -m5000 -i0 # these should NOT show up
. $srcdir/diag.sh tcpflood -p13514 -m10000 -i5000
. $srcdir/diag.sh tcpflood -p13515 -m500 -i15000 # these should NOT show up
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 5000 14999
exit_test
