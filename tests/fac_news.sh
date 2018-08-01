#!/bin/bash
# added 2014-09-17 by Rgerhards

# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(type="string" name="outfmt" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
if prifilt("news.*") then
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
. $srcdir/diag.sh tcpflood -m1000 -P 57
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 999 
exit_test
