#!/bin/bash
# test many concurrent tcp connections
# addd 2016-03-28 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:9:16:%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
. $srcdir/diag.sh tcpflood -m9
shutdown_when_empty
wait_shutdown
seq_check  0 8
exit_test
