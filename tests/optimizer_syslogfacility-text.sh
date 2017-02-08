#!/bin/bash
# add 2017-02-08 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $syslogfacility-text == "local4" then
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1000 -y
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 999

. $srcdir/diag.sh exit
