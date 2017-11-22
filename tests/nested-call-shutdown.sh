#!/bin/bash
# addd 2017-10-18 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="rs3" queue.type="linkedList") {
	action(type="omfile" template="outfmt" file="rsyslog.out.log")
}

ruleset(name="rs2" queue.type="linkedList") {
  call rs3
}

ruleset(name="rs1" queue.type="linkedList") {
  call rs2
  :omtesting:sleep 0 1000
}

if $msg contains "msgnum:" then call rs1
'
. $srcdir/diag.sh startup
#. $srcdir/diag.sh tcpflood -p13514 -m10000
. $srcdir/diag.sh injectmsg 0 1000
#. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh shutdown-immediate
. $srcdir/diag.sh wait-shutdown
# wo do not check reception - the main point is that we do not abort. The actual
# message count is unknown (as the point is to shut down while still in processing).
. $srcdir/diag.sh exit
