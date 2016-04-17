#!/bin/bash
# addd 2016-03-22 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%$!msgnum%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

action(type="mmjsonparse" cookie="")
if $parsesuccess == "OK" then {
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}
'
rm -f rsyslog.out.log	# do cleanup of previous subtest
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m 5000 "-j \" \""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
