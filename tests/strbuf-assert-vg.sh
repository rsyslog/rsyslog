#!/bin/bash
# Added 2018-01-17 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="list"){
    property(name="jsonmesg")
    constant(value="\n")
}
local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
'

. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m1 -y
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
export EXPECTED='"msg": "msgnum:00000000:", '
. $srcdir/diag.sh grep-check
. $srcdir/diag.sh exit
