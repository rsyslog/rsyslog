#!/bin/bash
# added 2018-04-16 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%$.msgnum%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514")

action(type="mmjsonparse" cookie="@cim:" container="$.")
if $parsesuccess == "OK" then {
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m 5000 -j "@cim: "
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit

