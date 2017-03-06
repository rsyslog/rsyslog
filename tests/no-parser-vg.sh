#!/bin/bash
# add 2017-03-06 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset")
ruleset(name="ruleset" parser="rsyslog.rfc5424") {
	action(type="omfile" file="rsyslog2.out.log")
}
'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
# note: we just check the valgrind output, the log file itself does not
# interest us

. $srcdir/diag.sh exit
