#!/bin/bash
# test many concurrent tcp connections
# addd 2016-03-28 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:9:16:%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m9
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check  0 8
. $srcdir/diag.sh exit
