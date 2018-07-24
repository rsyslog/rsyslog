#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2013-11-22

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[mmpstrucdata.sh\]: testing mmpstrucdata
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")
module(load="../plugins/imtcp/.libs/imtcp")

template(name="outfmt" type="string" string="%$!rfc5424-sd!tcpflood@32473!msgnum%\n")

input(type="imtcp" port="13514")

action(type="mmpstrucdata")
if $msg contains "msgnum" then
	action(type="omfile" template="outfmt" file="rsyslog.out.log")
'
startup_vg
sleep 1
. $srcdir/diag.sh tcpflood -m100 -y
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
seq_check 0 99
exit_test
