#!/bin/bash
. $srcdir/diag.sh init
generate_conf
add_conf '
#module(load="../plugins/imtcp/.libs/imtcp")
#input(type="imtcp" port="13514")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if `echo $DO_WORK` == "on" and $msg contains "msgnum:" then
	action(type="omfile" file=`echo $OUTFILE` template="outfmt")
'
export DO_WORK=on
export OUTFILE=rsyslog.out.log
startup_vg
. $srcdir/diag.sh injectmsg 0 1000
#. $srcdir/diag.sh tcpflood -m10
shutdown_when_empty
wait_shutdown_vg
seq_check  0 999
exit_test
