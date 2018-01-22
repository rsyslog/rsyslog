#!/bin/bash
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
#module(load="../plugins/imtcp/.libs/imtcp")
#input(type="imtcp" port="13514")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if `echo $DO_WORK` == "on" and $msg contains "msgnum:" then
	action(type="omfile" file=`echo $OUTFILE` template="outfmt")
'
export DO_WORK=on
export OUTFILE=rsyslog.out.log
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh injectmsg 0 1000
#. $srcdir/diag.sh tcpflood -m10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh seq-check  0 999
. $srcdir/diag.sh exit
