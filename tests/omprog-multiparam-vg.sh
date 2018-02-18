#!/bin/bash
# added 2016-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
#uname
#if [ `uname` = "SunOS" ] ; then
   #echo "Solaris: FIX ME"
   #exit 77
#fi

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omprog/.libs/omprog")
template(name="outfmt" type="string" string="%msg%\n")
action(type="omprog"
	binary="./testsuites/omprog-noterm.sh unused parameters"
	template="outfmt" name="omprog_action")

'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg  0 10
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh content-check "msgnum:00000009:"

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
./msleep 500
. $srcdir/diag.sh assert-content-missing "received SIGTERM"
. $srcdir/diag.sh content-check "PROCESS TERMINATED (last msg: Exit due to read-failure)"
. $srcdir/diag.sh exit
